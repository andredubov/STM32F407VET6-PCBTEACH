#include "stm32f407xx.h"
#include "delay.h"
#include "button.h"
#include "task.h"

#define READING_LEDS_STATE_TIME_MS              1000
#define RUNNING_LEDS_IDS_FROM_W25Q64_TIME_MS     100

volatile uint32_t system_tick = 1;
volatile static event_id_t event_id = NONE;

void SysTick_Handler(void)
{
    static uint8_t debounce_counter = 0;
    static uint16_t reading_leds_state_counter = 0;
    static uint16_t reading_leds_ids_from_w25q64_counter = 0;

    system_tick++;
    debounce_counter++;
    reading_leds_state_counter++;
    reading_leds_ids_from_w25q64_counter++;

    // Вызываем обработчик кнопок каждые DEBOUNCE_TIME_MS (10 мс)
    if (debounce_counter >= DEBOUNCE_TIME_MS) {
        debounce_counter = 0;
        buttons_debounce_handler();
    }

    // Вызываем обработчик каждые READING_LEDS_STATE_TIME_MS (1 с)
    if (reading_leds_state_counter >= READING_LEDS_STATE_TIME_MS) {
        reading_leds_state_counter = 0;
        event_id = RUNNING_LEDS_FROM_AT24C02_EVENT;
    }

    if (reading_leds_ids_from_w25q64_counter >= RUNNING_LEDS_IDS_FROM_W25Q64_TIME_MS) {
        reading_leds_ids_from_w25q64_counter = 0;
        event_id = RUNNING_LEDS_FROM_W25Q64_EVENT;
    }
}

uint32_t get_tick_ms(void)
{
    return system_tick;
}

event_id_t get_event_id(void)
{
    event_id_t event = event_id;
    event_id = NONE;  // Сброс после чтения
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
    
    if (reload_value > 0xFFFFFF) {
        reload_value = 0xFFFFFF;  // максимальное значение
    }
    
    SysTick->LOAD = reload_value;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    
    NVIC_SetPriority(SysTick_IRQn, 0x0F);
}

void delay_ms(uint32_t milliseconds)
{
    uint32_t start_tick = system_tick;

    while ( (system_tick - start_tick) < milliseconds ) {
        __NOP();  // Ожидание прерывания (экономит энергию)
    }
}