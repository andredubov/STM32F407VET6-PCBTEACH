#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>
#include "command.h"

// Enum для стандартных скоростей UART
typedef enum {
    UART_BAUDRATE_9600 = 9600,
    UART_BAUDRATE_19200 = 19200,
    UART_BAUDRATE_38400 = 38400,
    UART_BAUDRATE_57600 = 57600,
    UART_BAUDRATE_115200 = 115200,
    UART_BAUDRATE_230400 = 230400,
    UART_BAUDRATE_460800 = 460800,
    UART_BAUDRATE_921600 = 921600,
    UART_BAUDRATE_2M = 2000000,
    UART_BAUDRATE_3M = 3000000
} uart_baudrate_t;

// Enum для ошибок UART
typedef enum {
    UART_OK = 0,
    UART_ERROR_INIT,
    UART_ERROR_TIMEOUT,
    UART_ERROR_BUSY,
    UART_ERROR_PARAM
} uart_error_t;

// Основные функции
void uart_init(void);
uart_error_t uart_set_baudrate(uart_baudrate_t baudrate);
uart_baudrate_t uart_get_current_baudrate(void);

// Функции отправки
void uart_send_byte(uint8_t byte);
uart_error_t uart_send_data(const uint8_t* data, uint32_t size);
uart_error_t uart_send_string(const char* str);
uart_error_t uart_send_line(const char* str);
uart_error_t uart_printf(const char* format, ...);
uart_error_t uart_printf_line(const char* format, ...);
bool uart_is_ready_to_send(void);
void uart_flush_buffers(void);

// Функции приема
uint8_t uart_receive_byte(void);
bool uart_is_data_received(void);
command_id_t get_command_id(void);

#endif // UART_H