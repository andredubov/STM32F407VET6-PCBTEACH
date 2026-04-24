#include <stdbool.h>
#include <stdint.h>
#include "at24c02.h"
#include "button.h"
#include "w25q64.h"
#include "delay.h"
#include "led.h"
#include "uart.h"
#include "task.h"
#include "spi.h"
#include "timer_capture.h"

#define EEPROM_BASE_ADDRESS       0
#define SAVE_POINT_CNT            5
#define TIMEOUT_250ms           250
#define TIMEOUT_100ms           100
#define BUFFER_LENGTH             3
#define BASE_ADDRESS       0x303030

volatile static uint8_t eeprom_offset = 0;
static uint8_t buffer[AT24C02_SIZE];

static volatile led_id_t current_led = NONE;
static volatile led_id_t previous_led = NONE;

void save_pressed_button_into_eeprom(led_id_t led_id)
{
    bool ok = at24c02_write_byte(EEPROM_BASE_ADDRESS+eeprom_offset, led_id);
    if (!ok) {
        uart_send_line("Failed to write from EEPROM");
        return;
    }

    eeprom_offset = (eeprom_offset + 1) % SAVE_POINT_CNT;
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
            if (buffer[i] >= LED_1 && buffer[i] <= LED_3) 
            {
                led_id_t led_id = (led_id_t) buffer[i];    
                led_on(led_id);
                delay_ms(TIMEOUT_250ms);
                led_off(led_id);
                delay_ms(TIMEOUT_250ms);
            }
        }
    }
}

void clear_eeprom(void)
{
    at24c02_erase_all();
    eeprom_offset = 0;
}

void save_led_id_into_eeprom(led_id_t led_id)
{
    w25q64_error_t w25q64_error;
    uint32_t target_address = BASE_ADDRESS;

    spi_disable();
    spi_set_8bit_mode();
    spi_enable();

    switch (led_id) {
        case LED_1:;
            target_address = BASE_ADDRESS + 0;
            break;
        case LED_2:
            target_address = BASE_ADDRESS + 1;
            break;
        case LED_3:
            target_address = BASE_ADDRESS + 2;
            break;
        default:
            target_address = BASE_ADDRESS;
            break;
    }

    w25q64_error = w25q64_update_data(target_address, &led_id, 1);
    if (w25q64_error != W25Q_OK) {
        uart_printf_line("cannot read byte at 0x%06X", target_address);
    }
}

void save_leds_ids_into_eeprom(led_id_t led_1_id, led_id_t led_2_id, led_id_t led_3_id)
{
    w25q64_error_t w25q64_error;
    uint32_t target_address = BASE_ADDRESS;
    uint16_t data;

    data = (uint16_t)(led_2_id << 8);
    data |= led_3_id;

    w25q64_error = w25q64_erase_sector_4KB(target_address);
    if (w25q64_error != W25Q_OK) {
        uart_printf_line("cannot erase byte at 0x%06X", target_address);
    }

    w25q64_error = w25q64_write_with_mode_switch(target_address, led_1_id, data);
    if (w25q64_error != W25Q_OK) {
        uart_printf_line("cannot write data with SPI mode switch at 0x%06X", target_address);
    }
}

void load_led_id_from_eeprom(led_id_t led_id)
{
    w25q64_error_t w25q64_error;
    uint32_t target_address = BASE_ADDRESS;

    switch (led_id) {
        case LED_1:
            target_address = BASE_ADDRESS + 0;
            break;
        case LED_2:
            target_address = BASE_ADDRESS + 1;
            break;
        case LED_3:
            target_address = BASE_ADDRESS + 2;
            break;
        default:
            target_address = BASE_ADDRESS;
            break;
    }

    uint8_t value;

    w25q64_error = w25q64_read_byte(target_address, &value);
    if (w25q64_error != W25Q_OK) {
        uart_printf_line("cannot read byte at 0x%06X", target_address);
    }

    previous_led = current_led;
    current_led = (led_id_t) value;
}

void switch_on_led()
{
    led_off(previous_led);
    led_on(current_led);
}

void start_time_measurement(void)
{
    reset_time_measurement();  // Сброс предыдущего измерения
    uart_printf_line("\n✅ Measurement started: press S2 again to measure interval");
}

void get_time_measurement(void)
{
    bool is_completed = is_time_measurement_completed();
    
    if (is_completed) {
        // Выводим результат измерения
        uint32_t interval_ms = get_interval_ms();
        float interval_seconds = get_interval_seconds();
        uart_printf_line("📊 Interval: %u мс (%.3f с)", interval_ms, interval_seconds);
    } else {
        uart_send_line("⚠️ Measurement already done, ignoring");
    }
}