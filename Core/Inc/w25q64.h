#ifndef W25Q64_H
#define W25Q64_H

#include <stdint.h>
#include <stdbool.h>

// Адресация W25Q64 (64 Mbit = 8 MByte)
#define W25Q64_SIZE             0x800000    // 8,388,608 байт (8 МБ)
#define W25Q64_PAGE_SIZE        256         // 256 байт на страницу
#define W25Q64_SECTOR_SIZE      4096        // 4 КБ на сектор
#define W25Q64_BLOCK_SIZE_32KB  32768       // 32 КБ на блок
#define W25Q64_BLOCK_SIZE_64KB  65536       // 64 КБ на блок

// Команды W25Q64
typedef enum {
    // Основные команды
    W25Q_CMD_WRITE_ENABLE      = 0x06,
    W25Q_CMD_WRITE_DISABLE     = 0x04,
    W25Q_CMD_READ_STATUS_REG1  = 0x05,
    W25Q_CMD_READ_STATUS_REG2  = 0x35,
    W25Q_CMD_READ_STATUS_REG3  = 0x15,
    W25Q_CMD_WRITE_STATUS_REG1 = 0x01,
    W25Q_CMD_WRITE_STATUS_REG2 = 0x31,
    W25Q_CMD_WRITE_STATUS_REG3 = 0x11,
    
    // Команды чтения
    W25Q_CMD_READ_DATA         = 0x03,     // до 50 МГц
    W25Q_CMD_FAST_READ         = 0x0B,     // до 133 МГц
    W25Q_CMD_READ_DUAL         = 0x3B,
    W25Q_CMD_READ_QUAD         = 0x6B,
    
    // Команды записи/стирания
    W25Q_CMD_PAGE_PROGRAM      = 0x02,
    W25Q_CMD_SECTOR_ERASE_4KB  = 0x20,
    W25Q_CMD_BLOCK_ERASE_32KB  = 0x52,
    W25Q_CMD_BLOCK_ERASE_64KB  = 0xD8,
    W25Q_CMD_CHIP_ERASE        = 0xC7,     // или 0x60
    
    // Команды для защиты
    W25Q_CMD_READ_UNIQUE_ID    = 0x4B,
    W25Q_CMD_READ_JEDEC_ID     = 0x9F,
    W25Q_CMD_READ_MANUF_DEV_ID = 0x90,
    W25Q_CMD_ENABLE_RESET      = 0x66,
    W25Q_CMD_RESET_DEVICE      = 0x99,
    W25Q_CMD_POWER_DOWN        = 0xB9,
    W25Q_CMD_RELEASE_POWER_DOWN = 0xAB,
    
    // Команды для Quad I/O
    W25Q_CMD_FAST_READ_QUAD    = 0xEB,
    W25Q_CMD_QUAD_PAGE_PROGRAM = 0x32
} w25q64_command_t;

// Битовая маска регистра статуса 1
typedef enum {
    W25Q_SR1_BUSY      = 0x01,   // бит 0: занятость
    W25Q_SR1_WEL       = 0x02,   // бит 1: разрешение записи
    W25Q_SR1_BP0       = 0x04,   // бит 2: защита блоков 0
    W25Q_SR1_BP1       = 0x08,   // бит 3: защита блоков 1
    W25Q_SR1_BP2       = 0x10,   // бит 4: защита блоков 2
    W25Q_SR1_BP3       = 0x20,   // бит 5: защита блоков 3
    W25Q_SR1_TB        = 0x40,   // бит 6: направление защиты (0=верхняя область)
    W25Q_SR1_SRP0      = 0x80    // бит 7: защита регистра статуса 0
} w25q64_status_bit_t;

// Результаты операций
typedef enum {
    W25Q_OK = 0,
    W25Q_ERROR_PARAM,
    W25Q_ERROR_TIMEOUT,
    W25Q_ERROR_BUSY,
    W25Q_ERROR_WRITE_PROTECT,
    W25Q_ERROR_ADDRESS,
    W25Q_ERROR_ERASE_FAILED
} w25q64_error_t;

// Структура для JEDEC ID (Manufacturer ID + Memory Type + Capacity)
typedef struct {
    uint8_t manufacturer_id;    // Должно быть 0xEF для Winbond
    uint8_t memory_type;        // Должно быть 0x40 для W25Q64
    uint8_t capacity;           // Должно быть 0x17 для 64 Mbit (8 MB)
} w25q64_jedec_id_t;

// Основные функции
void w25q64_init(void);
bool w25q64_is_present(void);

// Чтение ID
w25q64_error_t w25q64_read_jedec_id(w25q64_jedec_id_t *id);
uint32_t w25q64_read_unique_id(void);
uint8_t w25q64_read_manufacturer_device_id(void);

// Регистры статуса
uint8_t w25q64_read_status_register1(void);
uint8_t w25q64_read_status_register2(void);
uint8_t w25q64_read_status_register3(void);
w25q64_error_t w25q64_write_status_register1(uint8_t value);
w25q64_error_t w25q64_write_status_register2(uint8_t value);
w25q64_error_t w25q64_write_status_register3(uint8_t value);

// Управление записью
void w25q64_write_enable(void);
void w25q64_write_disable(void);
bool w25q64_is_busy(void);
void w25q64_wait_for_ready(uint32_t timeout_ms);

// Основные операции
w25q64_error_t w25q64_read_data(uint32_t address, uint8_t *buffer, uint32_t size);
w25q64_error_t w25q64_fast_read_data(uint32_t address, uint8_t *buffer, uint32_t size);
w25q64_error_t w25q64_page_program(uint32_t address, const uint8_t *data, uint16_t size);
w25q64_error_t w25q64_write_buffer(uint32_t address, const uint8_t *data, uint32_t size);

// Стирание
w25q64_error_t w25q64_sector_erase(uint32_t address);
w25q64_error_t w25q64_block_erase_32kb(uint32_t address);
w25q64_error_t w25q64_block_erase_64kb(uint32_t address);
w25q64_error_t w25q64_chip_erase(void);

// Утилиты
void w25q64_power_down(void);
void w25q64_release_power_down(void);
void w25q64_reset(void);
w25q64_error_t w25q64_erase_all(void);

// Функции для отладки
void w25q64_print_status(void);
void w25q64_print_jedec_id(void);

#endif // W25Q64_H