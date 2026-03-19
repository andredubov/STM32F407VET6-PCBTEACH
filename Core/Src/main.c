#include "main.h"

int main(void)
{
    uint32_t mcu_frequency_khz = 84000u;

    rcc_init();                 // настройка тактирования
    leds_init();                // настройка светодиодов
    buttons_init();             // настройка кнопок
    uart_init();                // настройка UART (по умолчанию 115200)
    delay_init(mcu_frequency_khz); // настройка SysTick

    __enable_irq();

    for (;;)
    {
        button_event_t event = get_button_event();

        switch (event)
        {
            case BUTTON_1_PRESSED:
                uart_send_byte('1');
                break;
            case BUTTON_2_PRESSED:
                uart_send_byte('2');
                break;
            case BUTTON_3_PRESSED:
                uart_send_byte('3');
                break;
            default:
                break;
        }

        command_id_t command_id = get_command_id();

        switch (command_id)
        {
            case TURN_LED_1_ON:
                led_on( LED_1);
                break;
            case TURN_LED_2_ON:
                led_on( LED_2);
                break;
            case TURN_LED_3_ON:
                led_on( LED_3);
                break;
            default:
                led_off( LED_1);
                led_off( LED_2);
                led_off( LED_3);
                break;
        }
    }
}