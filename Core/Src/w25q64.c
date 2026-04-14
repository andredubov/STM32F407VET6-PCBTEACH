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

// Вспомогательная функция: отправка команды без данных
static void w25q64_send_command(uint8_t command)
{
    spi_cs_select();
    spi_transmit_byte(command);
    spi_cs_deselect();
}

// Вспомогательная функция: отправка команды с адресом (24 бита)
// static void w25q64_send_command_with_address(uint8_t command, uint32_t address)
// {
//     spi_cs_select();
//     spi_transmit_byte(command);
//     spi_transmit_byte((address >> 16) & 0xFF);
//     spi_transmit_byte((address >> 8) & 0xFF);
//     spi_transmit_byte(address & 0xFF);
//     spi_cs_deselect();
// }

// Инициализация W25Q64
void w25q64_init(void)
{
    if (is_initialized) {
        return;
    }
    
    uart_send_line("Initializing W25Q64...");

    w25q64_reset();
    
    // Небольшая задержка после включения питания
    delay_ms(10);
    
    // Проверяем наличие чипа
    if ( w25q64_is_present() ) {
        uart_send_line("W25Q64 detected successfully");
        // Получаем JEDEC ID для отладки
        w25q64_print_jedec_id();        
        // Выводим статус
        w25q64_print_status();
    } else {
        uart_send_line("WARNING: W25Q64 not detected!");
    }
    
    is_initialized = true;
}

// Проверка наличия чипа (чтение Manufacturer ID)
bool w25q64_is_present(void)
{
    uint8_t ids[3], dummy;
    spi_error_t error;

    uart_send_line("Reading JEDEC ID...");
    
    spi_cs_select();

    // Отправка команды
    spi_transmit_byte(W25Q_CMD_READ_JEDEC_ID);

    error = spi_transmit_receive_byte(0xFF, &dummy);
    if (error != SPI_OK) {
        spi_cs_deselect();
        return false;
    }

    // Чтение 3 байт ID
    error = spi_transmit_receive_byte(0xFF,&ids[0]); // Manufacturer ID
    if (error != SPI_OK) {
        spi_cs_deselect();
        return false;
    }    
    error = spi_transmit_receive_byte(0xFF,&ids[1]); // Memory Type
    if (error != SPI_OK) {
        spi_cs_deselect();
        return false;
    }
    error = spi_transmit_receive_byte(0xFF,&ids[2]); // Capacity
    if (error != SPI_OK) {
        spi_cs_deselect();
        return false;
    }

    spi_cs_deselect();

    uart_printf_line("JEDEC ID: 0x%02X 0x%02X 0x%02X", ids[0], ids[1], ids[2]);
    
    // Winbond (0xEF) W25Q64 (0x4017)
    return (ids[0] == 0xEF && ids[1] == 0x40 && ids[2] == 0x17);
}

// Чтение JEDEC ID
w25q64_error_t w25q64_read_jedec_id(w25q64_jedec_id_t *id)
{
    uint8_t dummy;

    if (!id) {
        return W25Q_ERROR_PARAM;
    }
    
    spi_cs_select();
    
    spi_transmit_byte(W25Q_CMD_READ_JEDEC_ID);
   
    spi_transmit_receive_byte(0xFF,&dummy);
    spi_transmit_receive_byte(0xFF,&id->manufacturer_id);
    spi_transmit_receive_byte(0xFF,&id->memory_type);
    spi_transmit_receive_byte(0xFF,&id->capacity);

    spi_cs_deselect();
    
    return W25Q_OK;
}

// Чтение уникального ID (64 бита)
uint32_t w25q64_read_unique_id(void)
{
    uint32_t unique_id = 0;
    
    spi_cs_select();
    delay_ms(1);
    spi_transmit_byte(W25Q_CMD_READ_UNIQUE_ID);
    
    // Пропускаем 4 байта (dummy)
    for (int i = 0; i < 4; i++) {
        uint8_t dummy;
        // spi_receive_byte(&dummy);
        spi_transmit_receive_byte(0xFF, &dummy);
        
    }
    
    // Читаем 4 байта уникального ID
    for (int i = 0; i < 4; i++) {
        uint8_t byte;
        // spi_receive_byte(&byte);
        spi_transmit_receive_byte(0xFF, &byte); 
        unique_id = (unique_id << 8) | byte;
    }
    
    spi_cs_deselect();
    
    return unique_id;
}

// Чтение Manufacturer и Device ID
uint8_t w25q64_read_manufacturer_device_id(void)
{
    uint8_t id;
    
    spi_cs_select();
    spi_transmit_byte(W25Q_CMD_READ_MANUF_DEV_ID);

    // Отправляем 24-битный адрес (0x000000)
    spi_transmit_byte(0x00);
    spi_transmit_byte(0x00);
    spi_transmit_byte(0x00);

    // Пропускаем 1 байт (dummy)
    uint8_t dummy;
    // spi_receive_byte(&dummy);
    spi_transmit_receive_byte(0xFF, &dummy);    
    
    // Читаем ID
    // spi_receive_byte(&id);
    spi_transmit_receive_byte(0xFF, &id);
    spi_cs_deselect();
    
    return id;
}

// Чтение регистра статуса 1
uint8_t w25q64_read_status_register1(void)
{
    uint8_t status;
    
    spi_cs_select();
    spi_transmit_byte(W25Q_CMD_READ_STATUS_REG1);
    spi_transmit_receive_byte(0xFF,&status);
    spi_cs_deselect();
    
    return status;
}

// Чтение регистра статуса 2
uint8_t w25q64_read_status_register2(void)
{
    uint8_t status;
    
    spi_cs_select();
    spi_transmit_byte(W25Q_CMD_READ_STATUS_REG2);
    spi_transmit_receive_byte(0xFF,&status);
    spi_cs_deselect();
    
    return status;
}

// Чтение регистра статуса 3
uint8_t w25q64_read_status_register3(void)
{
    uint8_t status;
    
    spi_cs_select();
    spi_transmit_byte(W25Q_CMD_READ_STATUS_REG3);
    spi_transmit_receive_byte(0xFF,&status);
    spi_cs_deselect();
    
    return status;
}

// Запись регистра статуса 1
w25q64_error_t w25q64_write_status_register1(uint8_t value)
{
    // Включаем запись
    w25q64_write_enable();
    
    // Записываем регистр
    spi_cs_select();
    spi_transmit_byte(W25Q_CMD_WRITE_STATUS_REG1);
    spi_transmit_byte(value);
    spi_cs_deselect();
    
    // Ждем завершения
    w25q64_wait_for_ready(W25Q64_TIMEOUT_MS);
    
    // Проверяем запись
    if (w25q64_read_status_register1() != value) {
        return W25Q_ERROR_WRITE_PROTECT;
    }
    
    return W25Q_OK;
}

// Запись регистра статуса 2
w25q64_error_t w25q64_write_status_register2(uint8_t value)
{
    w25q64_write_enable();
    
    spi_cs_select();
    spi_transmit_byte(W25Q_CMD_WRITE_STATUS_REG2);
    spi_transmit_byte(value);
    spi_cs_deselect();
    
    w25q64_wait_for_ready(W25Q64_TIMEOUT_MS);
    
    if (w25q64_read_status_register2() != value) {
        return W25Q_ERROR_WRITE_PROTECT;
    }
    
    return W25Q_OK;
}

// Запись регистра статуса 3
w25q64_error_t w25q64_write_status_register3(uint8_t value)
{
    w25q64_write_enable();
    
    spi_cs_select();
    spi_transmit_byte(W25Q_CMD_WRITE_STATUS_REG3);
    spi_transmit_byte(value);
    spi_cs_deselect();
    
    w25q64_wait_for_ready(W25Q64_TIMEOUT_MS);
    
    if (w25q64_read_status_register3() != value) {
        return W25Q_ERROR_WRITE_PROTECT;
    }
    
    return W25Q_OK;
}

// Включение записи
void w25q64_write_enable(void)
{
    w25q64_send_command(W25Q_CMD_WRITE_ENABLE);
}

// Отключение записи
void w25q64_write_disable(void)
{
    w25q64_send_command(W25Q_CMD_WRITE_DISABLE);
}

// Проверка занятости чипа
bool w25q64_is_busy(void)
{
    return (w25q64_read_status_register1() & W25Q_SR1_BUSY) != 0;
}

// Ожидание готовности чипа
void w25q64_wait_for_ready(uint32_t timeout_ms)
{
    uint32_t start_tick = get_tick_ms();
    
    while (w25q64_is_busy()) {
        if ((get_tick_ms() - start_tick) > timeout_ms) {
            uart_printf_line("W25Q64 timeout waiting for ready");
            return;
        }
        delay_ms(1);
    }
}

// Чтение данных
w25q64_error_t w25q64_read_data(uint32_t address, uint8_t *buffer, uint32_t size)
{
    if (!buffer || size == 0) {
        return W25Q_ERROR_PARAM;
    }
    
    if (address + size > W25Q64_SIZE) {
        uart_printf_line("Error: Read exceeds memory size (addr=0x%06X, size=%lu)", address, size);
        return W25Q_ERROR_ADDRESS;
    }
    
    spi_cs_select();
    spi_transmit_byte(W25Q_CMD_READ_DATA);
    spi_transmit_byte((address >> 16) & 0xFF);
    spi_transmit_byte((address >> 8) & 0xFF);
    spi_transmit_byte(address & 0xFF);
    
    for (uint32_t i = 0; i < size; i++) {
        spi_receive_byte(&buffer[i]);
    }
    
    spi_cs_deselect();
    
    uart_printf("Read %lu bytes from 0x%06X\n", size, address);
    
    return W25Q_OK;
}

// Быстрое чтение данных (с dummy байтом)
w25q64_error_t w25q64_fast_read_data(uint32_t address, uint8_t *buffer, uint32_t size)
{
    if (!buffer || size == 0) {
        return W25Q_ERROR_PARAM;
    }
    
    if (address + size > W25Q64_SIZE) {
        return W25Q_ERROR_ADDRESS;
    }
    
    spi_cs_select();
    spi_transmit_byte(W25Q_CMD_FAST_READ);
    spi_transmit_byte((address >> 16) & 0xFF);
    spi_transmit_byte((address >> 8) & 0xFF);
    spi_transmit_byte(address & 0xFF);
    
    // Отправляем dummy байт
    uint8_t dummy;
    spi_transmit_receive_byte(0xFF, &dummy);
    
    for (uint32_t i = 0; i < size; i++) {
        spi_receive_byte(&buffer[i]);
    }
    
    spi_cs_deselect();
    
    return W25Q_OK;
}

// Программирование страницы (максимум 256 байт)
w25q64_error_t w25q64_page_program(uint32_t address, const uint8_t *data, uint16_t size)
{
    if (!data || size == 0 || size > W25Q64_PAGE_SIZE) {
        uart_printf_line("Error: Invalid page program parameters (size=%d)", size);
        return W25Q_ERROR_PARAM;
    }
    
    if (address + size > W25Q64_SIZE) {
        uart_printf_line("Error: Page program exceeds memory (addr=0x%06X)", address);
        return W25Q_ERROR_ADDRESS;
    }
    
    // Включаем запись
    w25q64_write_enable();
    
    // Отправляем команду Page Program
    spi_cs_select();
    spi_transmit_byte(W25Q_CMD_PAGE_PROGRAM);
    spi_transmit_byte((address >> 16) & 0xFF);
    spi_transmit_byte((address >> 8) & 0xFF);
    spi_transmit_byte(address & 0xFF);
    
    // Отправляем данные
    for (uint16_t i = 0; i < size; i++) {
        spi_transmit_byte(data[i]);
    }
    
    spi_cs_deselect();
    
    // Ждем завершения записи
    w25q64_wait_for_ready(W25Q64_PAGE_TIMEOUT_MS);
    
    return W25Q_OK;
}

// Запись буфера произвольного размера (с разбивкой по страницам)
w25q64_error_t w25q64_write_buffer(uint32_t address, const uint8_t *data, uint32_t size)
{
    if (!data || size == 0) {
        return W25Q_ERROR_PARAM;
    }
    
    if (address + size > W25Q64_SIZE) {
        uart_printf_line("Error: Write exceeds memory size!");
        return W25Q_ERROR_ADDRESS;
    }
    
    uint32_t bytes_written = 0;
    uint32_t current_addr = address;
    
    uart_printf("Writing %lu bytes to W25Q64 at address 0x%06X...\n", size, address);
    
    while (bytes_written < size) {
        // Определяем смещение внутри страницы
        uint16_t page_offset = current_addr & (W25Q64_PAGE_SIZE - 1);
        uint16_t bytes_to_write = W25Q64_PAGE_SIZE - page_offset;
        
        if (bytes_to_write > (size - bytes_written)) {
            bytes_to_write = size - bytes_written;
        }
        
        // Записываем страницу
        w25q64_error_t result = w25q64_page_program(current_addr, &data[bytes_written], bytes_to_write);
        if (result != W25Q_OK) {
            uart_printf_line("Failed to write page at address 0x%06X", current_addr);
            return result;
        }
        
        bytes_written += bytes_to_write;
        current_addr += bytes_to_write;
    }
    
    uart_printf_line("Successfully wrote %lu bytes", size);
    
    return W25Q_OK;
}

// Стирание сектора (4 КБ)
w25q64_error_t w25q64_sector_erase(uint32_t address)
{
    if (address >= W25Q64_SIZE) {
        return W25Q_ERROR_ADDRESS;
    }
    
    // Выравнивание по границе сектора
    address &= ~(W25Q64_SECTOR_SIZE - 1);
    
    uart_printf("Erasing sector at 0x%06X (4KB)...\n", address);
    
    w25q64_write_enable();
    
    spi_cs_select();
    spi_transmit_byte(W25Q_CMD_SECTOR_ERASE_4KB);
    spi_transmit_byte((address >> 16) & 0xFF);
    spi_transmit_byte((address >> 8) & 0xFF);
    spi_transmit_byte(address & 0xFF);
    spi_cs_deselect();
    
    w25q64_wait_for_ready(W25Q64_SECTOR_TIMEOUT_MS);
    
    uart_send_line("Sector erase completed");
    
    return W25Q_OK;
}

// Стирание блока 32 КБ
w25q64_error_t w25q64_block_erase_32kb(uint32_t address)
{
    if (address >= W25Q64_SIZE) {
        return W25Q_ERROR_ADDRESS;
    }
    
    // Выравнивание по границе блока 32KB
    address &= ~(W25Q64_BLOCK_SIZE_32KB - 1);
    
    uart_printf("Erasing 32KB block at 0x%06X...\n", address);
    
    w25q64_write_enable();
    
    spi_cs_select();
    spi_transmit_byte(W25Q_CMD_BLOCK_ERASE_32KB);
    spi_transmit_byte((address >> 16) & 0xFF);
    spi_transmit_byte((address >> 8) & 0xFF);
    spi_transmit_byte(address & 0xFF);
    spi_cs_deselect();
    
    w25q64_wait_for_ready(W25Q64_BLOCK_TIMEOUT_MS);
    
    uart_send_line("32KB block erase completed");
    
    return W25Q_OK;
}

// Стирание блока 64 КБ
w25q64_error_t w25q64_block_erase_64kb(uint32_t address)
{
    if (address >= W25Q64_SIZE) {
        return W25Q_ERROR_ADDRESS;
    }
    
    // Выравнивание по границе блока 64KB
    address &= ~(W25Q64_BLOCK_SIZE_64KB - 1);
    
    uart_printf("Erasing 64KB block at 0x%06X...\n", address);
    
    w25q64_write_enable();
    
    spi_cs_select();
    spi_transmit_byte(W25Q_CMD_BLOCK_ERASE_64KB);
    spi_transmit_byte((address >> 16) & 0xFF);
    spi_transmit_byte((address >> 8) & 0xFF);
    spi_transmit_byte(address & 0xFF);
    spi_cs_deselect();
    
    w25q64_wait_for_ready(W25Q64_BLOCK_TIMEOUT_MS);
    
    uart_send_line("64KB block erase completed");
    
    return W25Q_OK;
}

// Полное стирание чипа
w25q64_error_t w25q64_chip_erase(void)
{
    uart_send_line("Erasing entire W25Q64 chip... This may take up to 30 seconds");
    
    w25q64_write_enable();
    w25q64_send_command(W25Q_CMD_CHIP_ERASE);
    
    w25q64_wait_for_ready(W25Q64_CHIP_TIMEOUT_MS);
    
    uart_send_line("Chip erase completed");
    
    return W25Q_OK;
}

// Переход в режим пониженного энергопотребления
void w25q64_power_down(void)
{
    w25q64_send_command(W25Q_CMD_POWER_DOWN);
    delay_ms(10);
}

// Выход из режима пониженного энергопотребления
void w25q64_release_power_down(void)
{
    w25q64_send_command(W25Q_CMD_RELEASE_POWER_DOWN);
    delay_ms(10);
}

// Сброс устройства
void w25q64_reset(void)
{
    w25q64_send_command(W25Q_CMD_ENABLE_RESET);
    w25q64_send_command(W25Q_CMD_RESET_DEVICE);
    delay_ms(10);
}

// Стереть всю память (аналог chip erase, но с проверкой)
w25q64_error_t w25q64_erase_all(void)
{
    return w25q64_chip_erase();
}

// Вывод статуса для отладки
void w25q64_print_status(void)
{
    uint8_t sr1 = w25q64_read_status_register1();
    uint8_t sr2 = w25q64_read_status_register2();
    uint8_t sr3 = w25q64_read_status_register3();
    
    uart_printf_line("W25Q64 Status Registers:");
    uart_printf_line("  SR1: 0x%02X (BUSY=%d, WEL=%d)", sr1, (sr1 >> 0) & 1, (sr1 >> 1) & 1);
    uart_printf_line("  SR2: 0x%02X", sr2);
    uart_printf_line("  SR3: 0x%02X", sr3);
}

// Вывод JEDEC ID для отладки
void w25q64_print_jedec_id(void)
{
    w25q64_jedec_id_t id;
    
    if ( w25q64_read_jedec_id(&id) == W25Q_OK ) 
    {
        uart_printf_line("JEDEC ID: 0x%02X 0x%02X 0x%02X", 
            id.manufacturer_id, 
            id.memory_type, 
            id.capacity
        );
    }
}