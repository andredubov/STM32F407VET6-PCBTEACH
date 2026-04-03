#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <stdbool.h>

// Режимы работы I2C
typedef enum {
    I2C_MODE_STANDARD = 100000,    // 100 кГц
    I2C_MODE_FAST = 400000         // 400 кГц
} i2c_speed_t;

// Статусы операций I2C
typedef enum {
    I2C_OK = 0,
    I2C_ERROR_BUSY,
    I2C_ERROR_TIMEOUT,
    I2C_ERROR_ARBITRATION_LOST,
    I2C_ERROR_ACK_FAILURE,
    I2C_ERROR_BUS_ERROR,
    I2C_ERROR_PARAM
} i2c_error_t;

// Структура для конфигурации I2C
typedef struct {
    uint32_t clock_speed;      // Скорость в Гц (100000 или 400000)
    uint8_t own_address;       // Собственный адрес устройства (7 бит)
    bool enable_ack;           // Включить ACK после каждого байта
    bool enable_general_call;  // Включить обращение по общему адресу (0x00)
} i2c_config_t;

// Основные функции
void i2c_init(void);
void i2c_init_with_config(const i2c_config_t *config);
i2c_error_t i2c_set_speed(i2c_speed_t speed);
i2c_error_t i2c_set_own_address(uint8_t address);


// Функции для работы в режиме Master
i2c_error_t i2c_master_transmit(
    uint8_t slave_addr,
    const uint8_t *data,
    uint32_t size,
    uint32_t timeout_ms
);

i2c_error_t i2c_master_receive(
    uint8_t slave_addr,
    uint8_t *buffer,
    uint32_t size,
    uint32_t timeout_ms
);

i2c_error_t i2c_master_write_then_read(
    uint8_t slave_addr,
    const uint8_t *write_data,
    uint32_t write_size,
    uint8_t *read_buffer,
    uint32_t read_size,
    uint32_t timeout_ms
);

#endif // I2C_H