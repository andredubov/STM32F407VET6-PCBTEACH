#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TURN_ALL_LEDS_OFF = 0,
    TURN_LED_1_ON,
    TURN_LED_2_ON,
    TURN_LED_3_ON
} CommandId_t;

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
} UartBaudrate_t;

// Enum для ошибок UART
typedef enum {
    UART_OK = 0,
    UART_ERROR_INIT,
    UART_ERROR_TIMEOUT,
    UART_ERROR_BUSY,
    UART_ERROR_PARAM
} UartError_t;

void uart_init();
UartError_t uart_set_baudrate(UartBaudrate_t baudrate);
UartBaudrate_t uart_get_current_baudrate(void);
void uart_send_byte(uint8_t byte);
UartError_t uart_send_data(const uint8_t* data, uint32_t size);
bool uart_is_ready_to_send(void);
uint8_t uart_receive_byte(void);
bool uart_is_data_received(void);
uint32_t get_apb2_frequency(void);  // Для отладки

CommandId_t get_command_id(void);


#endif // UART_H