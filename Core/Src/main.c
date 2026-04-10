#include "main.h"

int main(void)
{
    uint32_t mcu_frequency_hz = 84000000u; // 84 МГц
    uint8_t i2c_device_address = 0xA0;

    rcc_init();     // настройка тактирования
    delay_init(mcu_frequency_hz); // настройка SysTick
    leds_init();    // настройка светодиодов
    buttons_init(); // настройка кнопок
    uart_init();    // настройка UART (по умолчанию 115200)
    i2c_init();     // настройка I2C (100 кГц)
    at24c02_init(i2c_device_address); // иницилизация модуля для взаимодействия с микросхемой AT24C02B

    __enable_irq();

    for (;;)
    {
        button_event_t button_event_id = get_button_event();

        switch (button_event_id) {
            case BUTTON_1_PRESSED:
                save_pressed_button_into_eeprom(LED_1);
                uart_send_line("button S1 pressed");
                break;
            case BUTTON_2_PRESSED:
                save_pressed_button_into_eeprom(LED_2);
                uart_send_line("button S2 pressed");
                break;
            case BUTTON_3_PRESSED:
                save_pressed_button_into_eeprom(LED_3);
                uart_send_line("button S3 pressed");
                break;
            default:
                break;
        }

        event_id_t event_id = get_event();

        if (RUNNING_LEDS_FROM_EEPROM == event_id) {
            runnig_leds_from_eeprom();
        }

        command_id_t command_id = get_command_id();

        switch (command_id) {
            case TURN_LED_1_ON:
                led_on(LED_1);
                uart_send_line("LED 1 turned ON");
                break;
            case TURN_LED_2_ON:
                led_on(LED_2);
                uart_send_line("LED 2 turned ON");
                break;
            case TURN_LED_3_ON:
                led_on(LED_3);
                uart_send_line("LED 3 turned ON");
                break;
            case TURN_ALL_LEDS_OFF:
                led_off(LED_1);
                led_off(LED_2);
                led_off(LED_3);
                uart_send_line("LED 1 turned OFF");
                uart_send_line("LED 2 turned OFF");
                uart_send_line("LED 3 turned OFF");
                break;
            case ERASE_EEPROM:
                clear_eeprom();
                break;
            default:
                break;
        }
    }
}