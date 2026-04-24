// timer_capture.c
#include "timer_capture.h"
#include "stm32f407xx.h"
#include "button.h"
#include "uart.h"
#include "delay.h"
#include "critical_section.h"

#define APB1_FREQUENCY  42000000UL
#define APB2_FREQUENCY  42000000UL
#define DEBOUNCE_TIMEOUT_MS  200UL

// Глобальная переменная для отладки (интервал в миллисекундах)
volatile uint32_t capture = 0;

// Статические переменные для управления измерением
static volatile uint32_t first_capture = 0;
static volatile uint32_t second_capture = 0;
static volatile bool measurement_started = false;
static volatile bool measurement_done = false;
static volatile bool is_new_measurement = false;
static volatile uint32_t last_capture_time = 0;  // Для защиты от дребезга

// Функция инициализации TIM2 как предделителя (только настройка, без включения)
void timer2_init_as_prescaler(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    // Сброс
    TIM2->CR1 = 0;
    TIM2->CR2 = 0;
    TIM2->DIER = 0;
    TIM2->SR = 0;
    TIM2->CNT = 0;

    TIM2->ARR = 1;
    
    // PSC = 41999 для получения 1 кГц
    TIM2->PSC = (APB1_FREQUENCY / 1000) - 1;  // 42 MHz / 42000 = 1 kHz
    
    // Включаем авто-перезагрузку
    TIM2->CR1 |= TIM_CR1_ARPE;
    
    // Счёт вверх
    TIM2->CR1 &= ~(TIM_CR1_DIR | TIM_CR1_CMS);
    
    // Генерация TRGO при обновлении (Update event)
    TIM2->CR2 &= ~TIM_CR2_MMS;
    TIM2->CR2 |= TIM_CR2_MMS_1;  // MMS = 010 (Update event used as TRGO)
    
    uart_send_line("TIM2 initialized as prescaler");
}

// Функция инициализации TIM1 в режиме захвата входа (только настройка, без включения)
void timer1_init_in_capture_mode(void)
{
    // 1. Включить тактирование TIM1
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

    // 2. Сбросить TIM1
    TIM1->CR1 = 0;
    TIM1->CR2 = 0;
    TIM1->SMCR = 0;
    TIM1->DIER = 0;
    TIM1->SR = 0;
    TIM1->CNT = 0;

    // 3. Настройка TIM1 на работу от TIM2_TRGO (ITR1)
    // Сначала очищаем биты SMS и TS
    TIM1->SMCR &= ~(TIM_SMCR_SMS | TIM_SMCR_TS);    
    // Устанавливаем External Clock Mode 1 (SMS = 0b111)
    TIM1->SMCR |= (TIM_SMCR_SMS_2 | TIM_SMCR_SMS_1 | TIM_SMCR_SMS_0);  // SMS = 111 (External Clock Mode 1)
    // Выбираем источник ITR1 (TIM2_TRGO) - TS = 0b001
    TIM1->SMCR |= TIM_SMCR_TS_0;  // Бит 4 = 1

    // TIM1 не использует предделитель
    TIM1->PSC = 0;
    TIM1->ARR = 0xFFFF;
    TIM1->CR1 |= TIM_CR1_ARPE;

    // 4. Настройка GPIO для кнопок
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;

    // PE11 -> TIM1_CH2 (S2)
    GPIOE->MODER &= ~GPIO_MODER_MODER11;
    GPIOE->MODER |= GPIO_MODER_MODER11_1; // Alternate function
    GPIOE->AFR[1] &= ~GPIO_AFRH_AFSEL11;
    GPIOE->AFR[1] |= GPIO_AFRH_AFSEL11_0; // AF1 for TIM1_CH2
    GPIOE->PUPDR &= ~GPIO_PUPDR_PUPDR11;
    GPIOE->PUPDR |= GPIO_PUPDR_PUPDR11_0;  // Pull-up
    GPIOE->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR11;  // High speed

    // 5. Настройка захвата
    // Сначала очищаем все биты
    TIM1->CCER &= ~(TIM_CCER_CC2E | TIM_CCER_CC2P | TIM_CCER_CC2NP);

    // Режим входов - установить CC2S и CC3S в 0b01
    TIM1->CCMR1 &= ~TIM_CCMR1_CC2S;
    TIM1->CCMR1 |= TIM_CCMR1_CC2S_0;       // CC2S = 01 (вход)

    // Фильтр для подавления дребезга (8 выборок)
    TIM1->CCMR1 |= (0x0F << TIM_CCMR1_IC2F_Pos);

    // Настройка канала 2 (S2) - захват по спаду
    TIM1->CCER |= TIM_CCER_CC2P;           // CC2P=1 (захват по спаду)
    TIM1->CCER &= ~TIM_CCER_CC2E;           // Выключить захват

    // 6. Включить прерывания
    TIM1->DIER |= TIM_DIER_CC2IE;
    TIM1->DIER |= TIM_DIER_CC3IE;

    // 7. Настройка NVIC
    NVIC_SetPriority(TIM1_CC_IRQn, 0x0A);
    NVIC_EnableIRQ(TIM1_CC_IRQn);

    uart_send_line("TIM1 initialized with falling edge capture");
}

// Запуск TIM2
void timer2_start(void)
{
    TIM2->CNT = 0;             // Сброс счетчика
    TIM2->CR1 |= TIM_CR1_CEN;  // Включить TIM2
}

// Остановка TIM2
void timer2_stop(void)
{
    TIM2->CR1 &= ~(TIM_CR1_CEN);    // Выключить TIM2
    TIM2->CNT = 0;                  // Сброс счетчика
}

void timer2_reset(void)
{
    // Остановить таймер    
    TIM2->CR1 &= ~TIM_CR1_CEN;
    
    // Сбросить счетчик
    TIM2->CNT = 0;
    
    // Сбросить флаги прерываний
    TIM2->SR = 0;
    
    // Запустить заново
    TIM2->CR1 |= TIM_CR1_CEN;
}

void timer1_reset(void)
{
    // Остановить таймер
    TIM1->CR1 &= ~TIM_CR1_CEN;

    // Выключить захват по каналу №2
    TIM1->CCER &= ~TIM_CCER_CC2E;
    
    // Сбросить счетчик
    TIM1->CNT = 0;
    
    // Сбросить флаги прерываний
    TIM1->SR = 0;

    // Включить захват по каналу №2
    TIM1->CCER |= TIM_CCER_CC2E;
    
    // Запустить заново
    TIM1->CR1 |= TIM_CR1_CEN;
}

// Запуск TIM1
void timer1_start(void)
{
    TIM1->CNT = 0;              // Сброс счетчика

    TIM1->CCER |= TIM_CCER_CC2E; // Включить захват по каналу №2

    TIM1->CR1 |= TIM_CR1_CEN;   // Включить TIM1
}

// Остановка TIM1
void timer1_stop(void)
{
    TIM1->CR1 &= ~TIM_CR1_CEN;  // Выключить TIM1

    TIM1->CCER &= ~TIM_CCER_CC2E; // Отключить захват по каналу №2

    TIM1->CNT = 0;              // Сброс счетчика
}

// Обработчик прерываний с защитой от дребезга
void TIM1_CC_IRQHandler(void)
{
    if (TIM1->SR & TIM_SR_CC2IF)
    {
        uint32_t current_time = TIM1->CCR2;
        uint32_t now = get_tick_ms();
        uint32_t timeout;

        uint32_t basepri = critical_enter_basp(); // Используем BASEPRI для защиты (оставляем SysTick)
        
        // Защита от дребезга
        if (now >= last_capture_time) {
            timeout = now - last_capture_time;
        } else {
            timeout = (SYSTEM_TIMER_MAX_VALUE - last_capture_time) + now;
        }
        
        if (timeout < DEBOUNCE_TIMEOUT_MS) {
            TIM1->SR &= ~TIM_SR_CC2IF;
            critical_exit_basp(basepri);
            return;
        }
        last_capture_time = now;

        if (measurement_started) 
        {
            if (!measurement_done)
            {
                // ✅ Завершаем измерение
                first_capture = 0;
                second_capture = current_time;
                capture = second_capture - first_capture;

                measurement_done = true;
                measurement_started = false;
            }
        } 
        else 
        {
            if (measurement_done) 
            {
                is_new_measurement = false;
            }
        }

        TIM1->SR &= ~TIM_SR_CC2IF;
        critical_exit_basp(basepri);
    }
}

// Получить интервал в секундах
float get_interval_seconds(void)
{
    float result;
    
    CRITICAL_SECTION_START();
    result = (float) capture / 1000.0;
    CRITICAL_SECTION_END();
    
    return result;
}

// Получить интервал в миллисекундах
uint32_t get_interval_ms(void)
{
    uint32_t result;
    
    CRITICAL_SECTION_START();
    result = capture;
    CRITICAL_SECTION_END();
    
    return result;
}

// Проверить, завершено ли измерение
bool is_time_measurement_completed(void)
{
    bool result;
    
    CRITICAL_SECTION_START();
    result = measurement_done && is_new_measurement;
    CRITICAL_SECTION_END();
    
    return result;
}

void reset_time_measurement(void)
{
    CRITICAL_SECTION_START();

    measurement_started = true;
    is_new_measurement = true;
    measurement_done = false;
    first_capture = 0;
    second_capture = 0;
    capture = 0;

    CRITICAL_SECTION_END();

    timer2_reset();
    timer1_reset();
}