#include "stm32f407xx.h"
#include "button.h"

volatile uint32_t system_tick = 0;

// Обработчик SysTick
void SysTick_Handler(void)
{
    system_tick++;

    // Вызов обработчика демпфирования каждые 10 мс
    if ( (system_tick % 10) == 0 ) 
    {
        buttons_debounce_handler();
    }
}

uint32_t get_tick(void)
{
    return system_tick;
}

void delay_init(uint32_t frequency_khz)
{
    // frequency_khz = 84000 (84 МГц в кГц)
    // Для 1 мс интервала нужно: (частота_в_герцах / 1000) - 1
    // 84 МГц = 84,000,000 Гц
    // 84,000,000 / 1000 = 84,000
    // LOAD = 84,000 - 1 = 83,999

    uint32_t reload_value = (frequency_khz) - 1;
    
    if (reload_value > 0xFFFFFF) {
        reload_value = 0xFFFFFF;  // Максимальное значение
    }
    
    SysTick->LOAD = reload_value; // ПРАВИЛЬНО: 84000 - 1 = 83999
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