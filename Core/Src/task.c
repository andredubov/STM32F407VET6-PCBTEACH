#include "at24c02.h"
#include "delay.h"
#include "led.h"
#include "uart.h"

#define EEPROM_BASE_ADDRESS       0
#define SAVE_POINT_CNT            5
#define TIMEOUT_250ms           250

volatile static uint8_t eeprom_offset = 0;
static uint8_t buffer[AT24C02_SIZE];

void save_pressed_button_into_eeprom(led_id_t led_id)
{
    bool ok = at24c02_write_byte(EEPROM_BASE_ADDRESS+eeprom_offset, led_id);
    if (!ok) {
        uart_send_line("Failed to write from EEPROM");
        return;
    }

    eeprom_offset = (eeprom_offset % (SAVE_POINT_CNT-1)) + 1;
}

void runnig_leds_from_eeprom(void)
{
    bool ok = at24c02_read_buffer(EEPROM_BASE_ADDRESS, buffer, SAVE_POINT_CNT);
    if (!ok) {
        uart_send_line("Failed to read from EEPROM");
        return;
    }

    for (uint8_t i = 0; i < SAVE_POINT_CNT; ++i)
    {
        if (buffer[i] != 0xFF)
        {
            led_id_t led_id = (led_id_t) buffer[i];
            
            led_on(led_id);
            delay_ms(TIMEOUT_250ms);
            led_off(led_id);
            delay_ms(TIMEOUT_250ms);
        }
    }
}

void clear_eeprom(void)
{
    at24c02_erase_all();
    eeprom_offset = 0;
}