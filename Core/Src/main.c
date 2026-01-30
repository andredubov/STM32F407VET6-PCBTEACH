#include "main.h"

int main(void)
{
    uint32_t mcu_frequency_khz = 84000u;

    rcc_init();                 // Настройка тактирования
    leds_init();                // Настройка светодиодов
    buttons_init();             // Настройка кнопок
    delay_init(mcu_frequency_khz);  // Настройка SysTick

    for (;;) 
    {
        ButtonEvent_t event = get_button_event();

        switch (event)
        {
            case BUTTON_1_PRESSED:
                led_toggle(LED_1);  // Переключаем LED1
                break;
            case BUTTON_2_PRESSED:
                led_toggle(LED_2);  // Переключаем LED2
                break;
            case BUTTON_3_PRESSED:
                led_toggle(LED_3);  // Переключаем LED3
                break;
            default:
                // __WFI(); // idle-режим для экономии энергии
                break;
        }
    }
}