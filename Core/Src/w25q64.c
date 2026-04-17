#include <string.h>
#include "w25q64.h"
#include "spi.h"
#include "delay.h"
#include "uart.h"

// Таймауты (в миллисекундах)
#define W25Q64_TIMEOUT_MS        5000   // 5 секунд для стирания
#define W25Q64_PAGE_TIMEOUT_MS   10     // 10 мс для страницы
#define W25Q64_SECTOR_TIMEOUT_MS 300    // 300 мс для сектора
#define W25Q64_BLOCK_TIMEOUT_MS  2000   // 2 секунды для блока
#define W25Q64_CHIP_TIMEOUT_MS   30000  // 30 секунд для чипа

// Статус инициализации
static bool is_initialized = false;

static uint8_t sector_buffer[W25Q64_SECTOR_SIZE];
// static uint8_t temp_buffer[W25Q64_SECTOR_SIZE];

static bool is_sector_changed(const uint8_t* old_data, const uint8_t* new_data, uint32_t offset, uint32_t size) 
{
    // Проверяем только изменяемую область
    for (uint32_t i = 0; i < size; i++) {
        if (old_data[offset + i] != new_data[i]) {
            return true;
        }
    }
    return false;
}

// Вспомогательная функция: отправка команды без данных
static w25q64_error_t w25q64_send_command(uint8_t command)
{
    spi_error_t spi_error;

    spi_cs_select();
    spi_error = spi_transmit_byte(command);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }
    spi_cs_deselect();

    return W25Q_OK;
}

// Инициализация W25Q64
w25q64_error_t w25q64_init(void)
{   
    bool is_present;
    w25q64_error_t w25q64_error;    

    if (is_initialized) {
        return W25Q_OK;
    }
    
    uart_send_line("Initializing W25Q64...");

    w25q64_error = w25q64_reset();
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    w25q64_error = w25q64_is_present(&is_present);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }
    
    // Проверяем наличие чипа
    if (is_present) {
        uart_send_line("W25Q64 detected successfully");
        // Получаем JEDEC ID для отладки
        w25q64_print_jedec_id();
        // Выводим статус
        w25q64_print_status();
    } else {
        uart_send_line("WARNING: W25Q64 not detected!");
    }
    
    is_initialized = true;

    return W25Q_OK;
}

// Проверка наличия чипа (чтение Manufacturer ID)
w25q64_error_t w25q64_is_present(bool *is_present)
{
    w25q64_error_t w25q64_error;
    w25q64_jedec_id_t w25q64_id;

    *is_present = false;

    uart_send_line("Reading JEDEC ID...");

    w25q64_error = w25q64_read_jedec_id(&w25q64_id);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    uart_printf_line("JEDEC ID: 0x%02X 0x%02X 0x%02X", 
        w25q64_id.manufacturer_id, 
        w25q64_id.memory_type, 
        w25q64_id.capacity
    );

    // Winbond (0xEF) W25Q64 (0x4017)
    *is_present = (
        w25q64_id.manufacturer_id == 0xEF && w25q64_id.memory_type == 0x40 && w25q64_id.capacity == 0x17
    );

    return W25Q_OK;
}

// Чтение JEDEC ID
w25q64_error_t w25q64_read_jedec_id(w25q64_jedec_id_t *id)
{
    spi_error_t spi_error;

    if (!id) {
        return W25Q_ERROR_PARAM;
    }
    
    spi_cs_select();
    
    spi_error = spi_transmit_byte(W25Q_CMD_READ_JEDEC_ID);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }
    
    spi_error = spi_transmit_receive_byte(0xFF, &id->manufacturer_id);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }
    
    spi_error = spi_transmit_receive_byte(0xFF, &id->memory_type);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }

    spi_error = spi_transmit_receive_byte(0xFF, &id->capacity);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }

    spi_cs_deselect();
    
    return W25Q_OK;
}

// Чтение уникального ID (64 бита)
w25q64_error_t w25q64_read_unique_id(uint32_t* unique_id)
{
    uint8_t dummy;
    spi_error_t error;

    spi_cs_select();

    error = spi_transmit_byte(W25Q_CMD_READ_UNIQUE_ID);
    if (error != SPI_OK) {
        return W25Q_ERROR_SPI_COMMUNICATION;
    }
    
    // Пропускаем 4 байта (dummy)
    for (int i = 0; i < 4; i++) {        
        error = spi_transmit_receive_byte(0xFF, &dummy);
        if (error != SPI_OK) {
            spi_cs_deselect();
            return W25Q_ERROR_SPI_COMMUNICATION;
        }
    }
    
    // Читаем 4 байта уникального ID
    for (int i = 0; i < 4; i++) {
        uint8_t byte;
        error = spi_transmit_receive_byte(0xFF, &byte); 
        if (error != SPI_OK) {
            spi_cs_deselect();
            return W25Q_ERROR_SPI_COMMUNICATION;
        }
        *unique_id = (*unique_id << 8) | byte;
    }
    
    spi_cs_deselect();
    
    return W25Q_OK;
}

// Чтение Manufacturer и Device ID
w25q64_error_t w25q64_read_manufacturer_device_id(uint8_t* device_id)
{
    spi_error_t error;

    spi_cs_select();

    error = spi_transmit_byte(W25Q_CMD_READ_MANUF_DEV_ID);
    if (error != SPI_OK) {
        spi_cs_deselect();
        return error;
    }

    // Отправляем 24-битный адрес (0x000000)
    error = spi_transmit_byte(0x00);
    if (error != SPI_OK) {
        spi_cs_deselect();
        return error;
    }
    error = spi_transmit_byte(0x00);
    if (error != SPI_OK) {
        spi_cs_deselect();
        return error;
    }
    error = spi_transmit_byte(0x00);
    if (error != SPI_OK) {
        spi_cs_deselect();
        return error;
    }

    // Пропускаем 1 байт (dummy)
    uint8_t dummy;
    error = spi_transmit_receive_byte(0xFF, &dummy);    
    if (error != SPI_OK) {
        spi_cs_deselect();
        return error;
    }
    // Читаем ID
    error = spi_transmit_receive_byte(0xFF, device_id);
    if (error != SPI_OK) {
        spi_cs_deselect();
        return error;
    }

    spi_cs_deselect();
    
    return W25Q_OK;
}

// Чтение регистра статуса 1
w25q64_error_t w25q64_read_status_register1(uint8_t *status)
{
    uint8_t dummy;
    spi_error_t error;
    
    spi_cs_select();

    error = spi_transmit_byte(W25Q_CMD_READ_STATUS_REG1);
    if (error) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }

    error = spi_transmit_receive_byte(0xFF,&dummy);
    if (error) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }
    error = spi_transmit_receive_byte(0xFF,status);
    if (error) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }

    spi_cs_deselect();
    
    return W25Q_OK;
}

// Чтение регистра статуса 2
w25q64_error_t w25q64_read_status_register2(uint8_t *status)
{
    uint8_t dummy;
    spi_error_t error;
    
    spi_cs_select();

    error = spi_transmit_byte(W25Q_CMD_READ_STATUS_REG2);
    if (error) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }

    error = spi_transmit_receive_byte(0xFF,&dummy);
        if (error) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }
    error = spi_transmit_receive_byte(0xFF,status);
    if (error) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }

    spi_cs_deselect();
    
    return W25Q_OK;
}

// Чтение регистра статуса 3
w25q64_error_t w25q64_read_status_register3(uint8_t *status)
{
    uint8_t dummy;
    spi_error_t error;

    spi_cs_select();
    
    error = spi_transmit_byte(W25Q_CMD_READ_STATUS_REG3);
    if (error) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }

    error = spi_transmit_receive_byte(0xFF,&dummy);
    if (error) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }
    error = spi_transmit_receive_byte(0xFF,status);
    if (error) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }

    spi_cs_deselect();
    
    return W25Q_OK;
}

// Запись регистра статуса 1
w25q64_error_t w25q64_write_status_register1(uint8_t value)
{
    w25q64_error_t w25q64_error;
    spi_error_t spi_error;

    // Включаем запись
    w25q64_error = w25q64_write_enable();
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }
    
    // Записываем регистр
    spi_cs_select();
    spi_error = spi_transmit_byte(W25Q_CMD_WRITE_STATUS_REG1);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return spi_error;
    }
    spi_error = spi_transmit_byte(value);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return spi_error;
    }
    spi_cs_deselect();
    
    // Ждем завершения
    w25q64_error = w25q64_wait_for_ready(W25Q64_TIMEOUT_MS);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }
    
    // Проверяем запись
    uint8_t sr1;    
    w25q64_error = w25q64_read_status_register1(&sr1);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    if (sr1 != value) {
        return W25Q_ERROR_WRITE_PROTECT;
    }
    
    return W25Q_OK;
}

// Запись регистра статуса 2
w25q64_error_t w25q64_write_status_register2(uint8_t value)
{
    w25q64_error_t w25q64_error;
    spi_error_t spi_error;

    w25q64_error = w25q64_write_enable();
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }
    
    spi_cs_select();
    spi_error = spi_transmit_byte(W25Q_CMD_WRITE_STATUS_REG2);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }
    spi_error = spi_transmit_byte(value);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }
    spi_cs_deselect();
    
    w25q64_error = w25q64_wait_for_ready(W25Q64_TIMEOUT_MS);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    // Проверяем запись
    uint8_t sr2;    
    w25q64_error = w25q64_read_status_register2(&sr2);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    if (sr2 != value) {
        return W25Q_ERROR_WRITE_PROTECT;
    }
    
    return W25Q_OK;
}

// Запись регистра статуса 3
w25q64_error_t w25q64_write_status_register3(uint8_t value)
{
    w25q64_error_t w25q64_error;
    spi_error_t spi_error;

    w25q64_error = w25q64_write_enable();
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }
    
    spi_cs_select();
    spi_error = spi_transmit_byte(W25Q_CMD_WRITE_STATUS_REG3);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return spi_error;
    }
    spi_error = spi_transmit_byte(value);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return spi_error;
    }
    spi_cs_deselect();

    w25q64_error = w25q64_wait_for_ready(W25Q64_TIMEOUT_MS);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    // Проверяем запись
    uint8_t sr3;    
    w25q64_error = w25q64_read_status_register3(&sr3);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    if (sr3 != value) {
        return W25Q_ERROR_WRITE_PROTECT;
    }

    return W25Q_OK;
}

// Включение записи
w25q64_error_t w25q64_write_enable(void)
{
    w25q64_error_t w25q64_error;
    
    w25q64_error = w25q64_send_command(W25Q_CMD_WRITE_ENABLE);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }
    
    // Проверяем, что WEL установлен
    uint8_t sr1;
    w25q64_error = w25q64_read_status_register1(&sr1);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }
    
    return (sr1 & W25Q_SR1_WEL) ? W25Q_OK : W25Q_ERROR_WRITE_PROTECT;
}

// Отключение записи
w25q64_error_t w25q64_write_disable(void)
{
    w25q64_error_t w25q64_error;
    
    w25q64_error = w25q64_send_command(W25Q_CMD_WRITE_DISABLE);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }
    
    // Проверяем, что WEL установлен
    uint8_t sr1;
    w25q64_error = w25q64_read_status_register1(&sr1);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }
    
    return (!(sr1 & W25Q_SR1_WEL)) ? W25Q_OK : W25Q_ERROR_WRITE_PROTECT;
}

// Проверка занятости чипа
w25q64_error_t w25q64_is_busy(bool *is_busy)
{
    uint8_t status;
    w25q64_error_t error;

    error = w25q64_read_status_register1(&status);
    if (error != W25Q_OK) {
        return error;
    }

    *is_busy = (status & W25Q_SR1_BUSY) != 0;

    return W25Q_OK;
}

// Ожидание готовности чипа
w25q64_error_t w25q64_wait_for_ready(uint32_t timeout_ms)
{
    uint32_t start_tick = get_tick_ms();
    bool is_busy = true;
    
    while (is_busy)
    {
        w25q64_error_t error = w25q64_is_busy(&is_busy);
        if (error != W25Q_OK) {
            return error;
        }

        if ((get_tick_ms() - start_tick) > timeout_ms) {
            uart_printf_line("W25Q64 timeout waiting for ready");
            return W25Q_ERROR_TIMEOUT;
        }

        delay_ms(1);
    }

    return W25Q_OK;
}

// Чтение одного байта
w25q64_error_t w25q64_read_byte(uint32_t address, uint8_t *buffer)
{
    if (!buffer || address >= W25Q64_SIZE) {
        return W25Q_ERROR_PARAM;
    }
    
    return w25q64_read_bytes(address, buffer, 1);
}

// Чтение нескольких байт
w25q64_error_t w25q64_read_bytes(uint32_t address, uint8_t *buffer, uint32_t size)
{   
    spi_error_t spi_error;
    uint32_t current_address = address;
    
    if (!buffer || size == 0 || address + size > W25Q64_SIZE) {
        return W25Q_ERROR_PARAM;
    }

    spi_cs_select();
    
    // Отправляем команду чтения (стандартный SPI READ)
    spi_error = spi_transmit_byte(W25Q_CMD_READ_DATA);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }

    // Отправляем 24-битный адрес
    spi_error = spi_transmit_byte((current_address >> 16) & 0xFF);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }
    spi_error = spi_transmit_byte((current_address >> 8) & 0xFF);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }
    spi_error = spi_transmit_byte(current_address & 0xFF);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return W25Q_ERROR_SPI_COMMUNICATION;
    }

    // Читаем данные
    for (uint32_t i = 0; i < size; i++) {
        spi_error = spi_transmit_receive_byte(0xFF, &buffer[i]);
        if (spi_error != SPI_OK) {
            spi_cs_deselect();
            return W25Q_ERROR_SPI_COMMUNICATION;
        }
    }
    
    spi_cs_deselect();

    return W25Q_OK;
}

// Запись одного байта
w25q64_error_t w25q64_write_byte(uint32_t address, uint8_t *buffer)
{
    if (!buffer || address >= W25Q64_SIZE) {
        return W25Q_ERROR_PARAM;
    }
    
    return w25q64_write_bytes(address, buffer, 1);
}

// Запись нескольких байт с автоматическим разбиением по страницам
w25q64_error_t w25q64_write_bytes(uint32_t address, uint8_t *buffer, uint32_t size)
{   
    w25q64_error_t w25q64_error;
    spi_error_t spi_error;
    uint32_t bytes_written = 0;
    uint32_t current_address = address;
    
    if (!buffer || size == 0 || address + size > W25Q64_SIZE) {
        return W25Q_ERROR_PARAM;
    }

    while (bytes_written < size) {
        // Вычисляем, сколько байт можно записать в текущую страницу
        uint32_t page_offset = current_address & (W25Q64_PAGE_SIZE - 1);  // Смещение внутри страницы
        uint32_t bytes_in_page = W25Q64_PAGE_SIZE - page_offset;
        uint32_t bytes_to_write = (size - bytes_written) < bytes_in_page ? (size - bytes_written) : bytes_in_page;
        
        // Включаем запись
        w25q64_error = w25q64_write_enable();
        if (w25q64_error != W25Q_OK) {
            return w25q64_error;
        }
        
        // Отправляем команду Page Program
        spi_cs_select();
        spi_error = spi_transmit_byte(W25Q_CMD_PAGE_PROGRAM);
        if (spi_error != SPI_OK) {
            spi_cs_deselect();
            return W25Q_ERROR_SPI_COMMUNICATION;
        }
        
        // Отправляем адрес
        spi_error = spi_transmit_byte((current_address >> 16) & 0xFF);
        if (spi_error != SPI_OK) {
            spi_cs_deselect();
            return W25Q_ERROR_SPI_COMMUNICATION;
        }
        spi_error = spi_transmit_byte((current_address >> 8) & 0xFF);
        if (spi_error != SPI_OK) {
            spi_cs_deselect();
            return W25Q_ERROR_SPI_COMMUNICATION;
        }
        spi_error = spi_transmit_byte(current_address & 0xFF);
        if (spi_error != SPI_OK) {
            spi_cs_deselect();
            return W25Q_ERROR_SPI_COMMUNICATION;
        }
        
        // Отправляем данные
        for (uint32_t i = 0; i < bytes_to_write; i++) {
            spi_error = spi_transmit_byte(buffer[bytes_written + i]);
            if (spi_error != SPI_OK) {
                spi_cs_deselect();
                return W25Q_ERROR_SPI_COMMUNICATION;
            }
        }
        spi_cs_deselect();
        
        // Ждем завершения записи страницы
        w25q64_error = w25q64_wait_for_ready(W25Q64_PAGE_TIMEOUT_MS);
        if (w25q64_error != W25Q_OK) {
            return w25q64_error;
        }
        
        // Переходим к следующей странице
        bytes_written += bytes_to_write;
        current_address += bytes_to_write;
    }

    return W25Q_OK;
}

w25q64_error_t w25q64_update_data_v1(uint32_t address, uint8_t* new_data, uint32_t size)
{
    if (!new_data || size == 0 || address + size > W25Q64_SIZE) {
        return W25Q_ERROR_PARAM;
    }

    w25q64_error_t w25q64_error;
    uint32_t sector_start = address & ~(W25Q64_SECTOR_SIZE - 1);

    // 1. Читаем весь сектор
    w25q64_error = w25q64_read_bytes(sector_start, sector_buffer, W25Q64_SECTOR_SIZE);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }
    
    // 2. Изменяем нужную область в буфере
    memcpy(sector_buffer + (address - sector_start), new_data, size);
    
    // 3. Стираем сектор
    w25q64_error = w25q64_erase_sector_4KB(sector_start);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    // 4. Записываем весь сектор заново (постранично)
    w25q64_error = w25q64_write_bytes(sector_start, sector_buffer, W25Q64_SECTOR_SIZE);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    return W25Q_OK;
}

w25q64_error_t w25q64_update_data(uint32_t address, uint8_t* new_data, uint32_t size)
{
    w25q64_error_t w25q64_error;
    uint32_t first_sector = address & ~(W25Q64_SECTOR_SIZE - 1);
    uint32_t last_sector = (address + size - 1) & ~(W25Q64_SECTOR_SIZE - 1);
    uint32_t bytes_processed = 0;
    
    if (!new_data || size == 0 || address + size > W25Q64_SIZE) {
        return W25Q_ERROR_PARAM;
    }
    
    // Обрабатываем каждый сектор
    for (uint32_t sector = first_sector; sector <= last_sector; sector += W25Q64_SECTOR_SIZE) {
        uint32_t sector_start = sector;
        uint32_t sector_offset = (sector == first_sector) ? (address - first_sector) : 0;
        uint32_t sector_size = W25Q64_SECTOR_SIZE - sector_offset;
        
        // Корректируем размер для последнего сектора
        if (sector == last_sector) {
            sector_size = size - bytes_processed;
        }
        
        // Читаем текущий сектор
        w25q64_error = w25q64_read_bytes(sector_start, sector_buffer, W25Q64_SECTOR_SIZE);
        if (w25q64_error != W25Q_OK) {
            return w25q64_error;
        }
        
        // Проверяем, есть ли реальные изменения
        bool is_changed = is_sector_changed(
            sector_buffer, 
            new_data + bytes_processed, 
            sector_offset, 
            sector_size
        );

        if (is_changed) {
            // Изменяем данные в буфере
            memcpy(sector_buffer + sector_offset, new_data + bytes_processed, sector_size);
            
            // Стираем сектор
            w25q64_error = w25q64_erase_sector_4KB(sector_start);
            if (w25q64_error != W25Q_OK) {
                return w25q64_error;
            }
            
            // Записываем обновленный сектор
            w25q64_error = w25q64_write_bytes(sector_start, sector_buffer, W25Q64_SECTOR_SIZE);
            if (w25q64_error != W25Q_OK) {
                return w25q64_error;
            }
        }
        
        bytes_processed += sector_size;
    }
    
    return W25Q_OK;
}

w25q64_error_t w25q64_update_data_16(uint32_t address, uint16_t* new_data, uint32_t size)
{
    return W25Q_OK;
}

w25q64_error_t w25q64_update_byte(uint32_t address, uint8_t* new_data, uint32_t size)
{
    w25q64_error_t w25q64_error;
    uint32_t first_sector = address & ~(W25Q64_SECTOR_SIZE - 1);
    uint32_t last_sector = (address + size - 1) & ~(W25Q64_SECTOR_SIZE - 1);
    uint32_t bytes_processed = 0;
    
    if (!new_data || size == 0 || address + size > W25Q64_SIZE) {
        return W25Q_ERROR_PARAM;
    }
    
    // Обрабатываем каждый сектор
    for (uint32_t sector = first_sector; sector <= last_sector; sector += W25Q64_SECTOR_SIZE) {
        uint32_t sector_start = sector;
        uint32_t sector_offset = (sector == first_sector) ? (address - first_sector) : 0;
        uint32_t sector_size = W25Q64_SECTOR_SIZE - sector_offset;
        
        // Корректируем размер для последнего сектора
        if (sector == last_sector) {
            sector_size = size - bytes_processed;
        }
        
        // Читаем текущий сектор
        w25q64_error = w25q64_read_bytes(sector_start, sector_buffer, W25Q64_SECTOR_SIZE);
        if (w25q64_error != W25Q_OK) {
            return w25q64_error;
        }
        
        // Проверяем, есть ли реальные изменения
        bool is_changed = is_sector_changed(
            sector_buffer, 
            new_data + bytes_processed, 
            sector_offset, 
            sector_size
        );

        if (is_changed) {
            // Изменяем данные в буфере
            memcpy(sector_buffer + sector_offset, new_data + bytes_processed, sector_size);
            
            // Стираем сектор
            w25q64_error = w25q64_erase_sector_4KB(sector_start);
            if (w25q64_error != W25Q_OK) {
                return w25q64_error;
            }
            
            // Записываем обновленный сектор
            w25q64_error = w25q64_write_bytes(sector_start, sector_buffer, W25Q64_SECTOR_SIZE);
            if (w25q64_error != W25Q_OK) {
                return w25q64_error;
            }
        }
        
        bytes_processed += sector_size;
    }
    
    return W25Q_OK;
}

// Стирание сектора (4 КБ)
w25q64_error_t w25q64_erase_sector_4KB(uint32_t address)
{
    spi_error_t spi_error;
    w25q64_error_t w25q64_error;

    if (address >= W25Q64_SIZE) {
        return W25Q_ERROR_ADDRESS;
    }

    // Выравнивание по границе сектора
    address &= ~(W25Q64_SECTOR_SIZE - 1);
    
    uart_printf("Erasing sector at 0x%06X (4KB)...\n", address);
    
    w25q64_error = w25q64_write_enable();
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }
    
    spi_cs_select();
    spi_error = spi_transmit_byte(W25Q_CMD_SECTOR_ERASE_4KB);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return spi_error;
    }
    spi_error = spi_transmit_byte((address >> 16) & 0xFF);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return spi_error;
    }
    spi_error = spi_transmit_byte((address >> 8) & 0xFF);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return spi_error;
    }
    spi_error = spi_transmit_byte(address & 0xFF);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return spi_error;
    }
    spi_cs_deselect();
    
    w25q64_error = w25q64_wait_for_ready(W25Q64_SECTOR_TIMEOUT_MS);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }
    
    uart_send_line("Sector erase completed");
    
    return W25Q_OK;
}

// Стирание блока 64 КБ
w25q64_error_t w25q64_erase_block_64kb(uint32_t address)
{
    spi_error_t spi_error;
    w25q64_error_t w25q64_error;

    if (address >= W25Q64_SIZE) {
        return W25Q_ERROR_ADDRESS;
    }
    
    // Выравнивание по границе блока 64KB
    address &= ~(W25Q64_BLOCK_SIZE - 1);
    
    uart_printf("Erasing 64KB block at 0x%06X...\n", address);
    
    w25q64_error = w25q64_write_enable();
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    spi_cs_select();
    spi_error = spi_transmit_byte(W25Q_CMD_BLOCK_ERASE_64KB);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return spi_error;
    }
    spi_error = spi_transmit_byte((address >> 16) & 0xFF);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return spi_error;
    }
    spi_error = spi_transmit_byte((address >> 8) & 0xFF);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return spi_error;
    }
    spi_error = spi_transmit_byte(address & 0xFF);
    if (spi_error != SPI_OK) {
        spi_cs_deselect();
        return spi_error;
    }
    spi_cs_deselect();
    
    w25q64_error = w25q64_wait_for_ready(W25Q64_BLOCK_TIMEOUT_MS);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    uart_send_line("64KB block erase completed");
    
    return W25Q_OK;
}

// Полное стирание чипа
w25q64_error_t w25q64_erase_chip(void)
{
    w25q64_error_t w25q64_error;

    uart_send_line("Erasing entire W25Q64 chip... This may take up to 30 seconds");
    
    w25q64_error = w25q64_write_enable();
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    w25q64_error = w25q64_send_command(W25Q_CMD_CHIP_ERASE);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }
    
    w25q64_error = w25q64_wait_for_ready(W25Q64_CHIP_TIMEOUT_MS);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }
    
    uart_send_line("Chip erase completed");
    
    return W25Q_OK;
}

// Переход в режим пониженного энергопотребления
w25q64_error_t w25q64_power_down(void)
{
    w25q64_error_t w25q64_error;

    w25q64_error = w25q64_send_command(W25Q_CMD_POWER_DOWN);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    delay_ms(10);

    return W25Q_OK;
}

// Выход из режима пониженного энергопотребления
w25q64_error_t w25q64_release_power_down(void)
{
    w25q64_error_t w25q64_error;

    w25q64_error = w25q64_send_command(W25Q_CMD_RELEASE_POWER_DOWN);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    delay_ms(10);

    return W25Q_OK;
}

// Сброс устройства
w25q64_error_t w25q64_reset(void) 
{
    w25q64_error_t w25q64_error;

    w25q64_error = w25q64_send_command(W25Q_CMD_ENABLE_RESET);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    w25q64_error = w25q64_send_command(W25Q_CMD_RESET_DEVICE);
    if (w25q64_error != W25Q_OK) {
        return w25q64_error;
    }

    delay_ms(10);

    return W25Q_OK;
}

// Вывод статуса для отладки
void w25q64_print_status(void)
{
    uint8_t sr1, sr2, sr3;
    w25q64_error_t w25q64_error;

    w25q64_error = w25q64_read_status_register1(&sr1);
    if (w25q64_error != W25Q_OK) {
        uart_printf_line("reading status register #1 error: %d", w25q64_error);
        return;
    }

    w25q64_error = w25q64_read_status_register2(&sr2);
    if (w25q64_error != W25Q_OK) {
        uart_printf_line("reading status register #2 error: %d", w25q64_error);
        return;
    }

    w25q64_error = w25q64_read_status_register3(&sr3);
    if (w25q64_error != W25Q_OK) {
        uart_printf_line("reading status register #3 error: %d", w25q64_error);
        return;
    }

    uart_printf_line("W25Q64 Status Registers:");
    uart_printf_line("  SR1: 0x%02X (BUSY=%d, WEL=%d)", sr1, (sr1 >> 0) & 1, (sr1 >> 1) & 1);
    uart_printf_line("  SR2: 0x%02X", sr2);
    uart_printf_line("  SR3: 0x%02X", sr3);
}

// Вывод JEDEC ID для отладки
void w25q64_print_jedec_id(void) 
{
    w25q64_jedec_id_t id;
    w25q64_error_t w25q64_error;

    w25q64_error = w25q64_read_jedec_id(&id);
    if (w25q64_error != W25Q_OK) {
        return;
    }

    uart_printf_line("JEDEC ID: 0x%02X 0x%02X 0x%02X", 
        id.manufacturer_id, 
        id.memory_type, 
        id.capacity
    );

    return;
}