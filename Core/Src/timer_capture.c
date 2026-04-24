// timer_capture.c
#include "timer_capture.h"
#include "stm32f407xx.h"
#include "button.h"
#include "uart.h"
#include "delay.h"

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
    uart_printf_line("  PSC = %lu, ARR = %lu", TIM2->PSC, TIM2->ARR);
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
    TIM1->CCER |= TIM_CCER_CC2E;           // Включить захват

    // 6. Включить прерывания
    TIM1->DIER |= TIM_DIER_CC2IE;
    // TIM1->DIER |= TIM_DIER_CC3IE;
    
    // 7. Настройка NVIC
    // NVIC_SetPriority(TIM1_CC_IRQn, 0x0C);
    NVIC_EnableIRQ(TIM1_CC_IRQn);
    
    uart_send_line("TIM1 initialized with falling edge capture");
    uart_printf_line("  CCER = 0x%08X", TIM1->CCER);
    uart_printf_line("  CCMR1 = 0x%08X, CCMR2 = 0x%08X", TIM1->CCMR1, TIM1->CCMR2);
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
    TIM2->CR1 &= ~(TIM_CR1_CEN);  // Выключить TIM2
    TIM2->CNT = 0;              // Сброс счетчика
}

// Сброс счетчика TIM2
void timer2_reset_counter(void)
{
    TIM2->CNT = 0;
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
    
    // Сбросить счетчик
    TIM1->CNT = 0;
    
    // Сбросить флаги прерываний
    TIM1->SR = 0;
    
    // Запустить заново
    TIM1->CR1 |= TIM_CR1_CEN;
}

// Запуск TIM1
void timer1_start(void)
{
    TIM1->CNT = 0;              // Сброс счетчика

    TIM1->CR1 |= TIM_CR1_CEN;   // Включить TIM1
}

// Остановка TIM1
void timer1_stop(void)
{
    TIM1->CR1 &= ~TIM_CR1_CEN;  // Выключить TIM1

    TIM1->CNT = 0;              // Сброс счетчика
}

// Сброс счетчика TIM1
void timer1_reset_counter(void)
{
    TIM1->CNT = 0;
}

// Обработчик прерываний с защитой от дребезга
void TIM1_CC_IRQHandler(void)
{
    if (TIM1->SR & TIM_SR_CC2IF)
    {
        uint32_t current_time = TIM1->CCR2;
        uint32_t now = get_tick_ms();
        uint32_t timeout;
        
        // ✅ Защита от дребезга: игнорируем нажатия чаще чем через 50 мс
        if (now >= last_capture_time) {
            timeout = now - last_capture_time;
        } else {
            timeout = (0xFFFFFF - last_capture_time) + now;
        }

        if (timeout < DEBOUNCE_TIMEOUT_MS) {
            TIM1->SR &= ~TIM_SR_CC2IF;
            return;
        }
        last_capture_time = now;

        if (!measurement_started)
        {
            // ✅ Новое измерение
            // Останавливаем таймеры для сброса
            TIM2->CR1 &= ~TIM_CR1_CEN;
            TIM1->CR1 &= ~TIM_CR1_CEN;
            
            // Сбрасываем счетчики
            TIM2->CNT = 0;
            TIM1->CNT = 0;
            TIM1->SR = 0;
            TIM2->SR = 0;
            
            // Запускаем таймеры заново
            TIM2->CR1 |= TIM_CR1_CEN;
            TIM1->CR1 |= TIM_CR1_CEN;
            
            first_capture = 0;  // Так как счетчик сброшен
            measurement_started = true;
            measurement_done = false;
            
            uart_printf_line("\n✅ Measurement started: press S2 again to measure interval");
        }
        else if (!measurement_done)
        {
            // ✅ Завершаем измерение
            second_capture = current_time;
            
            // Расчет интервала с учетом переполнения
            if (second_capture >= first_capture)
            {
                capture = second_capture - first_capture;
            }
            else
            {
                capture = (0xFFFF - first_capture) + second_capture;
            }

            measurement_done = true;
            measurement_started = false;
            
            // Останавливаем таймеры
            TIM2->CR1 &= ~TIM_CR1_CEN;
            TIM1->CR1 &= ~TIM_CR1_CEN;

            uart_printf_line("📊 Interval: %lu ms (%lu.%03lu sec) [first=%lu, second=%lu]", 
                capture,
                capture / 1000,
                capture % 1000,
                first_capture, 
                second_capture
            );
        }
        else
        {
            uart_send_line("⚠️ Measurement already done, ignoring");
        }

        TIM1->SR &= ~TIM_SR_CC2IF;
    }
}

// Получить интервал в секундах
float get_interval_seconds(void)
{
    return (float) capture / 1000.0;
}

// Получить интервал в миллисекундах
uint32_t get_interval_ms(void)
{
    return capture;
}

// Проверить, завершено ли измерение
bool is_measurement_completed(void)
{
    return measurement_done;
}

// Сбросить измерение для следующей пары нажатий
// Исправленная функция сброса измерения
void reset_measurement(void)
{
    bool tim2_was_running = (TIM2->CR1 & TIM_CR1_CEN) != 0;
    bool tim1_was_running = (TIM1->CR1 & TIM_CR1_CEN) != 0;
    
    measurement_started = false;
    measurement_done = false;
    first_capture = 0;
    second_capture = 0;
    capture = 0;
    
    // Сбрасываем таймеры, сохраняя их состояние
    if (tim2_was_running) {
        TIM2->CR1 &= ~TIM_CR1_CEN;
        TIM2->CNT = 0;
        TIM2->CR1 |= TIM_CR1_CEN;
    } else {
        TIM2->CNT = 0;
    }
    
    if (tim1_was_running) {
        TIM1->CR1 &= ~TIM_CR1_CEN;
        TIM1->CNT = 0;
        TIM1->CR1 |= TIM_CR1_CEN;
    } else {
        TIM1->CNT = 0;
    }
}

void test_timer2(void)
{
    uart_send_line("Testing TIM2...");
    timer2_start();
    delay_ms(1000);
    uart_printf_line("TIM2 CNT after 1s: %lu", TIM2->CNT);
    timer2_stop();
}

void test_timer1_ticking(void)
{
    uart_send_line("Testing TIM1 external clock from TIM2...");
    
    // Сброс и запуск
    timer2_start();
    timer1_start();
    
    delay_ms(1000);
    
    uart_printf_line("TIM1 CNT after 1s: %lu (should be ~1000)", TIM1->CNT);
    
    timer1_stop();
    timer2_stop();
}

void test_all_itr_sources(void)
{
    const char* itr_names[] = {"ITR0 (TIM1_TRGO)", "ITR1 (TIM2_TRGO)", "ITR2 (TIM3_TRGO)", "ITR3 (TIM4_TRGO)"};
    uint32_t ts_values[] = {0, TIM_SMCR_TS_0, TIM_SMCR_TS_1, TIM_SMCR_TS_0 | TIM_SMCR_TS_1};
    
    for (int i = 0; i < 4; i++) {
        // Сброс TIM1
        TIM1->CR1 = 0;
        TIM1->CNT = 0;
        
        // Настройка slave mode
        TIM1->SMCR &= ~(TIM_SMCR_SMS | TIM_SMCR_TS);
        TIM1->SMCR |= TIM_SMCR_SMS_2;  // External Clock Mode 1
        TIM1->SMCR |= ts_values[i];
        
        // Запуск
        timer2_start();
        TIM1->CR1 |= TIM_CR1_CEN;
        
        delay_ms(1000);
        
        uart_printf_line("%s: TIM1 CNT = %lu", itr_names[i], TIM1->CNT);
        
        // Остановка
        TIM1->CR1 &= ~TIM_CR1_CEN;
        timer2_stop();
        TIM1->CNT = 0;
        
        delay_ms(100);
    }
}

void test_timer1_internal(void)
{
    uart_send_line("Testing TIM1 with internal prescaler...");
    
    // Перенастраиваем TIM1 для теста
    TIM1->CR1 = 0;
    TIM1->CNT = 0;
    TIM1->PSC = 42000 - 1;
    TIM1->CR1 |= TIM_CR1_CEN;
    
    delay_ms(1000);
    
    uart_printf_line("TIM1 CNT after 1s: %lu (should be ~1000)", TIM1->CNT);
    
    TIM1->CR1 &= ~TIM_CR1_CEN;
}

void test_timer1_counting(void)
{
    uart_send_line("Testing TIM1 counting (should count milliseconds)...");
    
    // Запускаем оба таймера
    timer2_start();  // TIM2 начинает генерировать 1 кГц
    timer1_start();  // TIM1 начинает считать импульсы
    
    delay_ms(1000);  // Ждём 1 секунду
    
    uint32_t cnt = TIM1->CNT;
    uart_printf_line("TIM1->CNT after 1s: %lu (expected ~1000)", cnt);
    
    timer1_stop();
    timer2_stop();
    
    if (cnt > 990 && cnt < 1010) {
        uart_send_line("✓ TIM1 correctly counts milliseconds!");
    } else {
        uart_printf_line("✗ TIM1 count incorrect: %lu", cnt);
    }
}

void diagnose_timer_connection(void)
{
    uart_send_line("\n=== DIAGNOSTICS ===");
    
    // 1. Проверяем, включено ли тактирование
    uart_send_line("\n[1] Clock enables:");
    uart_printf_line("  RCC_APB1ENR_TIM2EN: %s", (RCC->APB1ENR & RCC_APB1ENR_TIM2EN) ? "YES" : "NO");
    uart_printf_line("  RCC_APB2ENR_TIM1EN: %s", (RCC->APB2ENR & RCC_APB2ENR_TIM1EN) ? "YES" : "NO");
    
    // 2. Проверяем настройки TIM2
    uart_send_line("\n[2] TIM2 registers:");
    uart_printf_line("  CR1: 0x%08X (CEN=%d)", TIM2->CR1, (TIM2->CR1 & TIM_CR1_CEN) ? 1 : 0);
    uart_printf_line("  PSC: %lu", TIM2->PSC);
    uart_printf_line("  ARR: %lu", TIM2->ARR);
    uart_printf_line("  CR2: 0x%08X", TIM2->CR2);
    uart_printf_line("  SR: 0x%08X", TIM2->SR);
    
    // 3. Проверяем настройки TIM1
    uart_send_line("\n[3] TIM1 registers:");
    uart_printf_line("  CR1: 0x%08X (CEN=%d)", TIM1->CR1, (TIM1->CR1 & TIM_CR1_CEN) ? 1 : 0);
    uart_printf_line("  SMCR: 0x%08X", TIM1->SMCR);
    uart_printf_line("  PSC: %lu", TIM1->PSC);
    uart_printf_line("  ARR: %lu", TIM1->ARR);
    uart_printf_line("  CNT: %lu", TIM1->CNT);
    
    // 4. Проверяем подключение ITR1
    uart_send_line("\n[4] ITR connections:");
    uart_printf_line("  TIM1_SMCR_TS: %lu (0=ITR0,1=ITR1,2=ITR2,3=ITR3)", (TIM1->SMCR & TIM_SMCR_TS) >> 4);
    uart_printf_line("  TIM2 is connected to ITR1 (should be TS=1)");
}

void test_timer2_output(void)
{
    uart_send_line("\nTesting TIM2 TRGO output...");
    
    // Убеждаемся, что ARR=1
    TIM2->ARR = 1;
    TIM2->PSC = 41999;
    TIM2->CR2 |= TIM_CR2_MMS_1;  // Update as TRGO
    
    // Включаем TIM2
    TIM2->CNT = 0;
    TIM2->CR1 |= TIM_CR1_CEN;
    
    // Ждём немного
    delay_ms(100);
    
    // Проверяем флаг обновления
    uart_printf_line("TIM2->SR_UIF: %d", (TIM2->SR & TIM_SR_UIF) ? 1 : 0);
    uart_printf_line("TIM2->CNT: %lu (should toggle between 0 and 1)", TIM2->CNT);
    
    TIM2->CR1 &= ~TIM_CR1_CEN;
}

void debug_tim1_counting(void)
{
    uart_send_line("\n=== TIM1 Debug ===");

    // Останавливаем таймеры, если они были запущены
    timer1_stop();
    timer2_stop();
    
    // Запускаем TIM2 и TIM1
    timer2_start();
    timer1_start();
    
    // Даём время посчитать
    delay_ms(100);
    
    // Проверяем CNT
    uart_printf_line("TIM1->CNT after 100ms: %lu", TIM1->CNT);
    uart_printf_line("TIM1->SR: 0x%08X", TIM1->SR);
    uart_printf_line("TIM1->CCR2: %lu", TIM1->CCR2);
    uart_printf_line("TIM1->CCR3: %lu", TIM1->CCR3);
    
    timer1_stop();
    timer2_stop();
}

void debug_tim1_capture_pins(void)
{
    uart_send_line("\n=== TIM1 Capture Pin Debug ===");
    
    // Читаем реальное состояние пинов
    uart_printf_line("GPIOE_IDR: 0x%08X", GPIOE->IDR);
    uart_printf_line("  PE10 (S1): %d", (GPIOE->IDR & GPIO_IDR_IDR_10) ? 1 : 0);
    uart_printf_line("  PE11 (S2): %d", (GPIOE->IDR & GPIO_IDR_IDR_11) ? 1 : 0);
    
    // Проверяем, настроены ли пины на альтернативную функцию
    uart_printf_line("GPIOE_AFRH: 0x%08X", GPIOE->AFR[1]);
    uart_printf_line("  PE10 AF: %lu", (GPIOE->AFR[1] >> GPIO_AFRH_AFSEL10_Pos) & 0xF);
    uart_printf_line("  PE11 AF: %lu", (GPIOE->AFR[1] >> GPIO_AFRH_AFSEL11_Pos) & 0xF);
    
    // Проверяем настройки TIM1 для захвата
    uart_printf_line("\nTIM1_CCER: 0x%08X", TIM1->CCER);
    uart_printf_line("  CC2E: %d, CC2P: %d", 
                     (TIM1->CCER & TIM_CCER_CC2E) ? 1 : 0,
                     (TIM1->CCER & TIM_CCER_CC2P) ? 1 : 0);
    uart_printf_line("  CC3E: %d, CC3P: %d",
                     (TIM1->CCER & TIM_CCER_CC3E) ? 1 : 0,
                     (TIM1->CCER & TIM_CCER_CC3P) ? 1 : 0);
    
    uart_printf_line("\nTIM1_CCMR1: 0x%08X", TIM1->CCMR1);
    uart_printf_line("  CC2S: %lu", (TIM1->CCMR1 & TIM_CCMR1_CC2S) >> 8);
    uart_printf_line("TIM1_CCMR2: 0x%08X", TIM1->CCMR2);
    uart_printf_line("  CC3S: %lu", (TIM1->CCMR2 & TIM_CCMR2_CC3S) >> 8);
    
    // Проверяем флаги захвата
    uart_printf_line("\nTIM1_SR: 0x%08X", TIM1->SR);
    uart_printf_line("  CC2IF: %d", (TIM1->SR & TIM_SR_CC2IF) ? 1 : 0);
    uart_printf_line("  CC3IF: %d", (TIM1->SR & TIM_SR_CC3IF) ? 1 : 0);
}