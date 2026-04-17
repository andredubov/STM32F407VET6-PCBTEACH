#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include <stdbool.h>

// Режимы работы SPI
typedef enum {
    SPI_MODE_0 = 0,  // CPOL=0, CPHA=0 (ведущий, данные по переднему фронту)
    SPI_MODE_1 = 1,  // CPOL=0, CPHA=1 (ведущий, данные по заднему фронту)
    SPI_MODE_2 = 2,  // CPOL=1, CPHA=0 (ведущий, данные по переднему фронту)
    SPI_MODE_3 = 3   // CPOL=1, CPHA=1 (ведущий, данные по заднему фронту)
} spi_mode_t;

// Размер данных
typedef enum {
    SPI_DATA_SIZE_8BIT = 0,
    SPI_DATA_SIZE_16BIT = 1
} spi_data_size_t;

// Скорость SPI (делители)
typedef enum {
    SPI_BAUDRATE_DIV_2   = 0,  // fPCLK / 2
    SPI_BAUDRATE_DIV_4   = 1,  // fPCLK / 4
    SPI_BAUDRATE_DIV_8   = 2,  // fPCLK / 8
    SPI_BAUDRATE_DIV_16  = 3,  // fPCLK / 16
    SPI_BAUDRATE_DIV_32  = 4,  // fPCLK / 32
    SPI_BAUDRATE_DIV_64  = 5,  // fPCLK / 64
    SPI_BAUDRATE_DIV_128 = 6,  // fPCLK / 128
    SPI_BAUDRATE_DIV_256 = 7   // fPCLK / 256
} spi_baudrate_t;

// Статусы операций
typedef enum {
    SPI_OK = 0,
    SPI_ERROR_BUSY,
    SPI_ERROR_TIMEOUT,
    SPI_ERROR_OVERRUN,
    SPI_ERROR_PARAM
} spi_error_t;

// Структура конфигурации SPI
typedef struct {
    spi_mode_t mode;                 // Режим SPI (0-3)
    spi_data_size_t data_size;       // Размер данных (8 или 16 бит)
    spi_baudrate_t baudrate;         // Скорость (делитель)
    bool msb_first;                  // true - MSB first, false - LSB first
    bool software_ssm;               // true - программное управление CS
} spi_config_t;

// Основные функции
void spi_init(void);
void spi_init_with_config(const spi_config_t *config);
spi_error_t spi_set_config(const spi_config_t *config);

// Управление CS (программное)
void spi_cs_select(void);
void spi_cs_deselect(void);
void spi_cs_set(bool select);

// 8-битные функции
spi_error_t spi_transmit_byte(uint8_t data);
spi_error_t spi_receive_byte(uint8_t *data);
spi_error_t spi_transmit_receive_byte(uint8_t tx_data, uint8_t *rx_data);

// 16-битные функции
spi_error_t spi_transmit_word(uint16_t data);
spi_error_t spi_receive_word(uint16_t *data);
spi_error_t spi_transmit_receive_word(uint16_t tx_data, uint16_t *rx_data);

// Универсальные функции
spi_error_t spi_transmit_data(void *data, spi_data_size_t size_in_bit);
spi_error_t spi_receive_data(void *data, spi_data_size_t size_in_bit);

spi_error_t spi_transmit_buffer(const void *tx_buffer, uint32_t size, spi_data_size_t size_in_bit);
spi_error_t spi_transmit_buffer(const void *tx_buffer, uint32_t size, spi_data_size_t size_in_bit);
spi_error_t spi_transmit_receive_buffer(
    const void *tx_buffer,
    void *rx_buffer,
    uint32_t size,
    spi_data_size_t size_in_bit
);

// Утилиты
void spi_enable(void);
void spi_disable(void);
bool spi_is_busy(void);
void spi_flush_rx(void);

#endif // SPI_H