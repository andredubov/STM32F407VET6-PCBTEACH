#include "at24c02.h"
#include "delay.h"
#include "i2c.h"
#include "uart.h"

static uint8_t device_address = AT24C02_DEFAULT_ADDR;
static uint32_t TIMEOUT_100ms  = 100;

void at24c02_init(uint8_t device_addr)
{
    device_address = device_addr;
}

void at24c02_set_device_address(uint8_t device_addr)
{
    device_address = device_addr;
}

// Ожидание завершения внутреннего цикла записи
void at24c02_wait_for_write(void)
{
    delay_ms(AT24C02_WRITE_CYCLE_MS);
}

// Проверка готовности устройства (опрос ACK)
bool at24c02_is_ready(void)
{
    uint8_t dummy;
    // Пытаемся прочитать 1 байт, чтобы проверить ACK
    i2c_error_t result = i2c_master_receive(device_address, &dummy, 1, TIMEOUT_100ms);

    return (result == I2C_OK);
}

bool at24c02_write_byte(uint16_t mem_addr, uint8_t data)
{
    if (mem_addr >= AT24C02_SIZE)  // 256
    {
        uart_printf_line("Error: Invalid address 0x%02X", mem_addr);
        return false;
    }
    
    // A2=A1=A0=0, поэтому адрес всегда 0xA0
    uint8_t tx_buffer[2];
    tx_buffer[0] = (uint8_t)mem_addr;  // младшие 8 бит
    tx_buffer[1] = data;
    
    uart_printf("Writing byte to 0x%02X: 0x%02X ... ", mem_addr, data);
    
    i2c_error_t result = i2c_master_transmit(device_address, tx_buffer, 2, TIMEOUT_100ms);
    
    if (result == I2C_OK)
    {
        uart_send_line("OK");
        at24c02_wait_for_write();
        return true;
    }
    else
    {
        uart_printf_line("FAIL (error: %s)", i2c_get_error_string(result));
        return false;
    }
}

// Запись страницы (до 8 байт за раз)
bool at24c02_write_page(uint16_t mem_addr, const uint8_t *data, uint8_t size)
{
    if (mem_addr >= AT24C02_SIZE || size == 0 || size > AT24C02_PAGE_SIZE) 
    {
        uart_printf_line("Error: Invalid parameters (addr=0x%02X, size=%d)", mem_addr, size);
        return false;
    }
    
    // Проверяем, что не выходим за границы страницы
    if ((mem_addr & (AT24C02_PAGE_SIZE - 1)) + size > AT24C02_PAGE_SIZE)
    {
        uart_printf_line("Warning: Cross page boundary! Will split into multiple writes");
        return false;
    }

    uint8_t addr_low = mem_addr & 0xFF; // младшие 8 бит

    // Буфер: [адрес] + [данные]
    uint8_t tx_buffer[AT24C02_PAGE_SIZE + 1];
    tx_buffer[0] = addr_low;
    for (uint8_t i = 0; i < size; i++)
    {
        tx_buffer[i + 1] = data[i];
    }
    
    uart_printf("Writing page at 0x%02X (%d bytes)... ", mem_addr, size);
    
    i2c_error_t result = i2c_master_transmit(device_address, tx_buffer, size + 1, TIMEOUT_100ms);
    if (result == I2C_OK) {
        uart_send_line("OK");
        at24c02_wait_for_write();
        return true;
    } else {
        uart_printf_line("FAIL (error: %s)", i2c_get_error_string(result));
        return false;
    }
}

// Запись произвольного количества данных (с разбивкой по страницам)
bool at24c02_write_buffer(uint16_t mem_addr, const uint8_t *data, uint16_t size)
{
    if (mem_addr + size > AT24C02_SIZE)
    {
        uart_printf_line("Error: Write exceeds memory size!");
        return false;
    }
    
    uint16_t bytes_written = 0;
    uint16_t current_addr = mem_addr;
    
    while (bytes_written < size)
    {
        // Определяем сколько байт можем записать в текущую страницу
        uint8_t page_offset = current_addr & (AT24C02_PAGE_SIZE - 1);
        uint8_t bytes_to_write = AT24C02_PAGE_SIZE - page_offset;
        
        if (bytes_to_write > (size - bytes_written))
        {
            bytes_to_write = size - bytes_written;
        }
        
        // Записываем страницу
        if (!at24c02_write_page(current_addr, &data[bytes_written], bytes_to_write))
        {
            uart_printf_line("Failed to write at address 0x%02X", current_addr);
            return false;
        }
        
        bytes_written += bytes_to_write;
        current_addr += bytes_to_write;
        
        // Небольшая задержка между страницами
        delay_ms(1);
    }
    
    uart_printf_line("Total %d bytes written successfully", size);

    return true;
}

// Чтение одного байта
bool at24c02_read_byte(uint16_t mem_addr, uint8_t *data)
{
    if (mem_addr >= AT24C02_SIZE || 0 == data) 
    {
        return false;
    }
    
    uint8_t addr_low = mem_addr & 0xFF; // младшие 8 бит
    
    // Сначала отправляем адрес
    i2c_error_t result = i2c_master_transmit(device_address, &addr_low, 1, TIMEOUT_100ms);
    if (result != I2C_OK)
    {
        uart_printf_line("Read error: failed to send address");
        return false;
    }
    
    // Затем читаем данные
    result = i2c_master_receive(device_address, data, 1, TIMEOUT_100ms);

    if (result == I2C_OK)
    {
        uart_printf("Read byte from 0x%02X: 0x%02X\n", mem_addr, *data);
        return true;
    }
    else
    {
        uart_printf_line("Read error: %s", i2c_get_error_string(result));
        return false;
    }
}

// Чтение нескольких байт
bool at24c02_read_buffer(uint16_t mem_addr, uint8_t *buffer, uint16_t size)
{
    if (mem_addr + size > AT24C02_SIZE || 0 == buffer || 0 == size)  {
        uart_printf_line("Read error: invalid parameters");
        return false;
    }
    
    uint8_t addr_low = mem_addr & 0xFF; // младшие 8 бит
    
    // Отправляем адрес для чтения
    i2c_error_t result = i2c_master_transmit(device_address, &addr_low, 1, TIMEOUT_100ms);
    if (result != I2C_OK) {
        uart_printf_line("Read error: failed to send address");
        return false;
    }

    // Читаем данные
    result = i2c_master_receive(device_address, buffer, size, TIMEOUT_100ms);
    if (result == I2C_OK) {
        // uart_printf_line("Read %d bytes from address 0x%02X", size, mem_addr);
        return true;
    } else {
        uart_printf_line("Read error: %s", i2c_get_error_string(result));
        return false;
    }
}

// Стереть всю память (заполнить 0xFF)
void at24c02_erase_all(void)
{
    uint8_t erase_buffer[AT24C02_PAGE_SIZE];
    
    // Заполняем буфер значением 0xFF
    for (uint8_t i = 0; i < AT24C02_PAGE_SIZE; i++) {
        erase_buffer[i] = 0xFF;
    }
    
    uart_send_line("Erasing AT24C02...");
    
    // Записываем страница за страницей
    for (uint16_t addr = 0; addr < AT24C02_SIZE; addr += AT24C02_PAGE_SIZE) 
    {
        if (!at24c02_write_page(addr, erase_buffer, AT24C02_PAGE_SIZE)) {
            uart_printf_line("Erase failed at address 0x%02X", addr);
            return;
        }
    }
    
    uart_send_line("Erasing completed!\n");
}