#include "at24c02.h"
#include "w25q64.h"
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

void test_w25q64(void)
{
    uint8_t test_data[256];
    uint8_t read_buffer[256];
    
    // Инициализация W25Q64
    w25q64_init();
    
    // Заполняем тестовые данные
    for (int i = 0; i < 256; i++) {
        test_data[i] = i;
    }
    
    // Стираем сектор перед записью
    w25q64_sector_erase(0x000000);
    
    // Записываем данные
    if (w25q64_write_buffer(0x000000, test_data, 256) == W25Q_OK) 
    {
        uart_send_line("Write successful");
    }
    
    // Читаем данные
    if (w25q64_read_data(0x000000, read_buffer, 256) == W25Q_OK) 
    {
        uart_send_line("Read successful");
        
        // Проверяем данные
        bool match = true;
        for (int i = 0; i < 256; i++) 
        {
            if (test_data[i] != read_buffer[i]) {
                match = false;
                uart_printf_line("Mismatch at position %d: expected 0x%02X, got 0x%02X",
                                 i, test_data[i], read_buffer[i]);
                break;
            }
        }
        
        if (match) {
            uart_send_line("Data verification PASSED!");
        } else {
            uart_send_line("Data verification FAILED!");
        }
    }
}