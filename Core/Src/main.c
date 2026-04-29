#include "main.h"

void system_init(void)
{
    uint32_t mcu_frequency_hz = 84000000u;
    uint8_t i2c_device_address = 0xA0;

    rcc_init();
    delay_init(mcu_frequency_hz);
    leds_init();
    buttons_init();
    uart_init();
    i2c_init();
    spi_init();
    at24c02_init(i2c_device_address);
    w25q64_init();
    timer2_init_as_prescaler();
    timer1_init_in_capture_mode();
    adc_init();
    dma_init();
}

void print_banner(void)
{
    uart_send_line("\r\n=========================================");
    uart_send_line("   STM32F407 System Ready");
    uart_send_line("=========================================");
    uart_send_line("Commands:");
    uart_send_line("  '1' '2' '3' - Turn on LEDs");
    uart_send_line("  '0' - Turn off all LEDs");
    uart_send_line("  '4' - Erase EEPROM");
    uart_send_line("  S1 - Start time measurement");
    uart_send_line("  S2 - Get time measurement");
    uart_send_line("=========================================\r\n");
}

int main(void)
{
    system_init();

    __enable_irq();
    
    adc_start();
    print_banner();
    copy_buffer_using_dma();
    send_buffer_into_uart_using_dma();

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

        adc_event_t adc_event = get_adc_event();

        switch (adc_event) {
            case ADC_EVENT_HIGH_THRESHOLD:
                led_on(LED_1);
                uart_send_line("⚠️ ADC Watchdog: Voltage above 80% threshold!");
                break;
            case ADC_EVENT_LOW_THRESHOLD:
                led_on(LED_2);
                uart_send_line("⚠️ ADC Watchdog: Voltage below 10% threshold!");
                break;
            case ADC_EVENT_WATCHDOG_NORMAL:
                led_off(LED_1);
                led_off(LED_2);
                uart_send_line("✅ ADC Watchdog: Voltage back to normal range");
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