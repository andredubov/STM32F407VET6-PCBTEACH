#include "stm32f407xx.h"
#include "delay.h"
#include "button.h"
#include "task.h"
#include "critical_section.h"

#define READING_LEDS_STATE_TIME_MS              1000
#define DMA_UART_DATA_TRANSFER_TIME_MS          1000
#define RUNNING_LEDS_IDS_FROM_W25Q64_TIME_MS     100

volatile uint32_t system_tick = 1;
volatile static event_id_t event_id = EVENT_NONE;

void SysTick_Handler(void)
{
    uint32_t basepri = critical_enter_basp(); // SysTick - самый высокий приоритет, используем BASEPRI

    static uint8_t debounce_counter = 0;
    static uint16_t dma_uart_data_transfer_couneter = 0;

    system_tick++;
    debounce_counter++;
    dma_uart_data_transfer_couneter++;

    // Вызываем обработчик кнопок каждые DEBOUNCE_TIME_MS (10 мс)
    if (debounce_counter >= DEBOUNCE_TIME_MS) {
        debounce_counter = 0;
        buttons_debounce_handler();
    }

    if (dma_uart_data_transfer_couneter >= DMA_UART_DATA_TRANSFER_TIME_MS) {
        dma_uart_data_transfer_couneter = 0;
        event_id = START_DMA_UART_DATA_TRANSFER_EVENT;
    }

    critical_exit_basp(basepri);
}

uint32_t get_tick_ms(void)
{
    uint32_t tick;
    
    // Защищаем чтение 32-битной переменной (атомарность не гарантирована)
    CRITICAL_SECTION_START();
    tick = system_tick;
    CRITICAL_SECTION_END();
    
    return tick;
}

event_id_t get_event_id(void)
{
    event_id_t event;
    
    CRITICAL_SECTION_START();
    event = event_id;
    event_id = EVENT_NONE;
    CRITICAL_SECTION_END();
    
    return event;
}

void delay_init(uint32_t frequency_hz)
{
    // frequency_hz = 84,000,000 (84 МГц в Гц)
    // Для 1 мс интервала нужно: (частота_в_герцах / 1000) - 1
    // 84 МГц = 84,000,000 Гц
    // 84,000,000 / 1000 = 84,000
    // LOAD = 84,000 - 1 = 83,999

    // Расчёт для прерывания каждые 1 мс
    uint32_t reload_value = (frequency_hz / 1000) - 1; // 84000 - 1 = 83999
    
    if (reload_value > SYSTEM_TIMER_MAX_VALUE) {
        reload_value = SYSTEM_TIMER_MAX_VALUE;  // максимальное значение
    }
    
    SysTick->LOAD = reload_value;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    
    NVIC_SetPriority(SysTick_IRQn, 0x0F);
}

// void delay_ms(uint32_t milliseconds)
// {
//     uint32_t start_tick = system_tick;

//     while ( (system_tick - start_tick) < milliseconds ) {
//         __NOP();  // Ожидание прерывания (экономит энергию)
//     }
// }

void delay_ms(uint32_t milliseconds)
{
    uint32_t start_tick = get_tick_ms();
    uint32_t elapsed;
    
    while (1) {
        elapsed = get_tick_ms() - start_tick;
        if (elapsed >= milliseconds) {
            break;
        }
        __NOP();
    }
}