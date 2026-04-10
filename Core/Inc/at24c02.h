#ifndef AT24C02_H
#define AT24C02_H

#include <stdint.h>
#include <stdbool.h>

// Адрес микросхемы AT24C02B (зависит от подключения выводов A0, A1, A2)
// Базовый адрес: 0x50 (1010000)
// С учетом A0, A1, A2: 0x50 | (A2<<2) | (A1<<1) | A0
#define AT24C02_DEFAULT_ADDR    0xA0    // A0=A1=A2=0

// Размер памяти AT24C02B
#define AT24C02_SIZE            256     // 256 байт
#define AT24C02_PAGE_SIZE       8       // 8 байт на страницу

// Время записи страницы (max 5ms)
#define AT24C02_WRITE_CYCLE_MS  5

// Функции инициализации
void at24c02_init(uint8_t device_addr);
void at24c02_set_device_address(uint8_t device_addr);

// Базовые операции
bool at24c02_write_byte(uint16_t mem_addr, uint8_t data);
bool at24c02_write_page(uint16_t mem_addr, const uint8_t *data, uint8_t size);
bool at24c02_write_buffer(uint16_t mem_addr, const uint8_t *data, uint16_t size);

bool at24c02_read_byte(uint16_t mem_addr, uint8_t *data);
bool at24c02_read_buffer(uint16_t mem_addr, uint8_t *buffer, uint16_t size);

// Утилиты
void at24c02_wait_for_write(void);
bool at24c02_is_ready(void);
void at24c02_erase_all(void);

#endif // AT24C02_H