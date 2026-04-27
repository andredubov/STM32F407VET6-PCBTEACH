#include "stm32f407xx.h"
#include "adc.h"
#include "delay.h"
#include "uart.h"
#include "critical_section.h"

#define ADC_VREF                    3300
#define ADC_RESOLUTION_10BIT        1024
#define ADC_TRIGGER_FREQUENCY_HZ    1000

#define ADC_THRESHOLD_LOW           (uint16_t)(ADC_RESOLUTION_10BIT * 0.1)
#define ADC_THRESHOLD_HIGH          (uint16_t)(ADC_RESOLUTION_10BIT * 0.8)
#define APB1_FREQUENCY              42000000UL

static volatile adc_event_t adc_event = ADC_EVENT_NONE;
static volatile bool last_watchdog_state = false;  // Для отслеживания изменений

void adc_init_timer_trigger(void)
{
    static bool timer_initialized = false;
    
    if (timer_initialized) {
        return;  // Уже инициализирован
    }

    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    TIM2->CR1 &= ~TIM_CR1_CEN;

    // Сброс
    TIM2->CR1 = 0;
    TIM2->CR2 = 0;
    TIM2->DIER = 0;
    TIM2->SR = 0;
    TIM2->CNT = 0;
    
    // PSC = 41999 для получения 1 кГц
    TIM2->PSC = (APB1_FREQUENCY / ADC_TRIGGER_FREQUENCY_HZ) - 1;
    TIM2->ARR = 1;

    // Включаем авто-перезагрузку
    TIM2->CR1 |= TIM_CR1_ARPE;

    // Счёт вверх
    TIM2->CR1 &= ~(TIM_CR1_DIR | TIM_CR1_CMS);
    
    // Генерация TRGO при обновлении (Update event)
    TIM2->CR2 &= ~TIM_CR2_MMS;
    TIM2->CR2 |= TIM_CR2_MMS_1; // MMS = 010 (Update event used as TRGO)
    
    uart_printf_line("TIM2 configured as ADC trigger source: %d Hz", ADC_TRIGGER_FREQUENCY_HZ);

    timer_initialized = true;
}

static void timer2_start(void)
{
    TIM2->CNT = 0;
    TIM2->CR1 |= TIM_CR1_CEN;
}

static void timer2_stop(void)
{
    TIM2->CR1 &= ~TIM_CR1_CEN;
    TIM2->CNT = 0;
}

void adc_init(void)
{
    static bool adc_initialized = false;
    
    if (adc_initialized) {
        uart_send_line("ADC already initialized");
        return;
    }

    // 1. Включение тактирования ADC1 и GPIOA
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    
    // 2. Настройка пина PA5 как аналоговый вход
    GPIOA->MODER |= GPIO_MODER_MODER5;      // 11 = Analog mode
    
    // 3. Настройка ADC1
    ADC1->CR2 = 0;
    ADC1->CR1 = 0;
    ADC1->SQR1 = 0;
    ADC1->SQR2 = 0;
    ADC1->SQR3 = 0;
    ADC1->JSQR = 0;
    
    // 4. Разрешение 10 бит (RES = 01)
    ADC1->CR1 &= ~ADC_CR1_RES_1;            // RES_1 = 0
    ADC1->CR1 |= ADC_CR1_RES_0;             // RES_0 = 1 -> 10 бит
    
    // 5. Время выборки для канала 5 = 15 циклов (SMP5 = 001)
    ADC1->SMPR2 &= ~ADC_SMPR2_SMP5;
    ADC1->SMPR2 |= ADC_SMPR2_SMP5_0;        // 001 = 15 циклов
    
    // 6. Настройка инжектированной последовательности
    //    JL[1:0] = 00 (1 конверсия в инжектированной последовательности)
    ADC1->JSQR &= ~ADC_JSQR_JL;
    ADC1->JSQR |= (0 << ADC_JSQR_JL_Pos);   // JL = 0 означает 1 конверсию
    
    //    JSQ4[4:0] = 5 (канал 5)
    ADC1->JSQR &= ~ADC_JSQR_JSQ4;
    ADC1->JSQR |= (5 << ADC_JSQR_JSQ4_Pos);
    
    // 7. Настройка Analog Watchdog
    //    Включить AWD для инжектированного канала
    ADC1->CR1 |= ADC_CR1_AWDIE;             // Разрешить прерывание AWD
    ADC1->CR1 |= ADC_CR1_JAWDEN;            // AWD для инжектированных каналов
    
    //    Установить пороговые значения
    ADC1->LTR = ADC_THRESHOLD_LOW;          // Нижний порог
    ADC1->HTR = ADC_THRESHOLD_HIGH;         // Верхний порог
    
    //    Включить AWD для канала 5 (AWDCH[4:0] = 5)
    ADC1->CR1 &= ~ADC_CR1_AWDCH;
    ADC1->CR1 |= (5 << ADC_CR1_AWDCH_Pos);
    
    // 8. Настройка триггера для инжектированной группы
    //    JEXTSEL[2:0] = 00 (TIM2_TRGO)
    ADC1->CR2 &= ~ADC_CR2_JEXTSEL;
    ADC1->CR2 |= (3 << ADC_CR2_JEXTSEL_Pos);    // 011 = TIM2_TRGO
    
    //    JEXTEN[1:0] = 01 (аппаратный триггер по возрастающему фронту)
    ADC1->CR2 &= ~ADC_CR2_JEXTEN;
    ADC1->CR2 |= ADC_CR2_JEXTEN_0;              // 01 = rising edge
    
    // 9. Включить прерывание по окончанию инжектированной группы (JEOC)
    ADC1->CR1 |= ADC_CR1_JEOCIE;
    
    // 10. Настройка NVIC для прерывания ADC
    NVIC_SetPriority(ADC_IRQn, 0x0C);
    NVIC_EnableIRQ(ADC_IRQn);
    
    // Инициализация переменных
    CRITICAL_SECTION_START();
    adc_event = ADC_EVENT_NONE;
    last_watchdog_state = false;
    CRITICAL_SECTION_END();
    
    uart_send_line("ADC1 initialized");
    uart_printf_line("  Thresholds: low=%lu, high=%lu", ADC_THRESHOLD_LOW, ADC_THRESHOLD_HIGH);

    adc_initialized = true;
}

void adc_start(void)
{
    ADC1->CR2 |= ADC_CR2_ADON;
    delay_ms(1);

    timer2_start();
    uart_send_line("ADC and TIM2 started, conversions triggered at 1 kHz");
}

void adc_stop(void)
{
    timer2_stop();
    ADC1->CR2 &= ~ADC_CR2_ADON;
    uart_send_line("ADC and TIM2 stopped");
}

// Обработчик прерывания ADC (генерирует события)
void ADC_IRQHandler(void)
{
    uint32_t basepri = critical_enter_basp();
    
    // Проверяем флаг окончания инжектированной группы (JEOC)
    if (ADC1->SR & ADC_SR_JEOC) {
        // Читаем результат инжектированного канала (для сброса флага)
        volatile uint32_t adc_value = ADC1->JDR1;
        (void)adc_value;

        // Проверяем состояние флага Analog Watchdog
        bool current_watchdog_state = (ADC1->SR & ADC_SR_AWD) != 0;

        // Сбрасываем флаги ДО генерации событий
        ADC1->SR &= ~(ADC_SR_JEOC | ADC_SR_AWD);

        if (current_watchdog_state != last_watchdog_state) {
            // Состояние изменилось - генерируем событие
            if (current_watchdog_state) {
                // Выход за пределы диапазона
                if (adc_value >= ADC_THRESHOLD_HIGH) {
                    adc_event = ADC_EVENT_HIGH_THRESHOLD;
                } else if (adc_value <= ADC_THRESHOLD_LOW) {
                    adc_event = ADC_EVENT_LOW_THRESHOLD;
                } else {
                    adc_event = ADC_EVENT_WATCHDOG_TRIGGERED;
                }
            } else {
                // Возврат в нормальный диапазон
                adc_event = ADC_EVENT_WATCHDOG_NORMAL;
            }
            last_watchdog_state = current_watchdog_state;
        }
    }
    
    critical_exit_basp(basepri);
}

bool adc_is_watchdog_triggered(void)
{
    adc_event_t event = get_adc_event();

    // Если получили событие триггера - возвращаем true
    if (event == ADC_EVENT_WATCHDOG_TRIGGERED) {
        return true;
    }

    // Иначе возвращаем текущее состояние (опционально)
    bool result;
    CRITICAL_SECTION_START();
    result = last_watchdog_state;
    CRITICAL_SECTION_END();

    return result;
}

// Функция получения события
adc_event_t get_adc_event(void)
{
    adc_event_t event;

    CRITICAL_SECTION_START();
    event = adc_event;
    adc_event = ADC_EVENT_NONE;  // Сбрасываем после чтения
    CRITICAL_SECTION_END();

    return event;
}

void adc_clear_watchdog_flag(void)
{
    CRITICAL_SECTION_START();
    adc_event = ADC_EVENT_NONE;
    CRITICAL_SECTION_END();
}