#include "main.h"
#include "delay.h"
#include "task.h"

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
    spi_init();     // настройка SPI (10.5 МГц до 133 МГц)
    at24c02_init(i2c_device_address); // иницилизация модуля для взаимодействия с микросхемой AT24C02B
    w25q64_init();  // иницилизация модуля для взаимодействия с микросхемой W25Q64
    timer2_init_as_prescaler(); // TIM2 - предделитель (1 кГц)
    timer1_init_in_capture_mode(); // TIM1 - захват входа

    __enable_irq();

    for (;;)
    {
        button_event_t button_event_id = get_button_event();

        switch (button_event_id) {
            case BUTTON_1_PRESSED:
                start_time_measurement();
                break;
            case BUTTON_2_PRESSED:
                get_time_measurement();
                break;
            case BUTTON_3_PRESSED:
                uart_send_line("button S3 pressed");
                break;
            default:
                break;
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

        __WFI();
    }
}