#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "command.h"
#include "stm32f407xx.h"
#include "delay.h"
#include "button.h"
#include "uart.h"
#include "critical_section.h"


// Частота APB2 для USART1 (42 МГц после настройки RCC)
#define APB2_FREQUENCY 42000000UL
#define TIMEOUT_2s     2000

// Внутренние переменные
static bool is_initialized = false;
static uart_baudrate_t current_baudrate = UART_BAUDRATE_115200;
volatile static command_id_t command_id = CMD_NONE;

// Буфер для форматированного вывода
static char print_buffer[128];

void uart_init(void)
{
    if (is_initialized) {
        return;
    }

    // 1. Включить тактирование
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    // 2. Настроить GPIO (TX=PA9, RX=PA10)
    GPIOA->MODER &= ~(GPIO_MODER_MODER9 | GPIO_MODER_MODER10);
    GPIOA->MODER |= (2 << GPIO_MODER_MODER9_Pos) | (2 << GPIO_MODER_MODER10_Pos);

    GPIOA->AFR[1] &= ~(GPIO_AFRH_AFSEL9 | GPIO_AFRH_AFSEL10);
    GPIOA->AFR[1] |= (7 << GPIO_AFRH_AFSEL9_Pos) | (7 << GPIO_AFRH_AFSEL10_Pos);

    // 3. Сбросить USART перед настройкой
    USART1->CR1 &= ~(USART_CR1_UE);  // Отключить USART для настройки

    // 4. Настроить скорость
    uart_error_t result = uart_set_baudrate(current_baudrate);
    if (UART_ERROR_PARAM == result) {
        return;
    }

    // 5. Настроить формат (8 бит, без четности, 1 стоп)
    USART1->CR1 &= ~(USART_CR1_M | USART_CR1_PCE);  // 8 бит, без контроля четности
    USART1->CR2 &= ~(USART_CR2_STOP);               // 1 стоповый бит

    // 6. Включить передатчик и приемник
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;     // Вкл. передатчик и приемник

    // 7. Включить USART
    USART1->CR1 |= USART_CR1_UE;

    // 8. Включить прерывания (если нужно)
    USART1->CR1 |= USART_CR1_RXNEIE;                // Прерывание по приему
    NVIC_EnableIRQ(USART1_IRQn);
    NVIC_SetPriority(USART1_IRQn, 0);

    is_initialized = true;
}

void uart_flush_buffers(void) 
{
    // 1. Отключаем USART
    USART1->CR1 &= ~USART_CR1_UE;
    
    // 2. Очищаем буфер RX (читаем весь мусор)
    while (USART1->SR & USART_SR_RXNE) {
        (void)USART1->DR;
    }
    
    // 3. Сбрасываем флаги статуса (TC, TXE, IDLE, и т.д.)
    USART1->SR = 0;
    
    // 4. Включаем USART обратно
    USART1->CR1 |= USART_CR1_UE;
}

uart_error_t uart_set_baudrate(uart_baudrate_t baudrate)
{
    if (baudrate < 9600 || baudrate > 3000000) {
        return UART_ERROR_PARAM;
    }

    // Сохранить текущую скорость
    current_baudrate = baudrate;
    
    // Таблица готовых значений BRR для APB2 = 42 MHz
    uint32_t brr_value;

    switch(baudrate) {
        case UART_BAUDRATE_9600:   brr_value = 0x1117; break;  // 273.6875 → 0x1117
        case UART_BAUDRATE_19200:  brr_value = 0x088B; break;  // 136.84375 → 0x088B
        case UART_BAUDRATE_38400:  brr_value = 0x0445; break;  // 68.421875 → 0x0445
        case UART_BAUDRATE_57600:  brr_value = 0x02DC; break;  // 45.609375 → 0x02DC
        case UART_BAUDRATE_115200: brr_value = 0x016C; break;  // 22.8046875 → 0x016C
        case UART_BAUDRATE_230400: brr_value = 0x00B6; break;  // 11.40234375 → 0x00B6
        case UART_BAUDRATE_460800: brr_value = 0x005B; break;  // 5.701171875 → 0x005B
        case UART_BAUDRATE_921600: brr_value = 0x002D; break;  // 2.8505859375 → 0x002D
        default:
            // Расчет для нестандартных скоростей
            uint32_t usartdiv = (APB2_FREQUENCY * 100) / (16 * baudrate);
            uint32_t mantissa = usartdiv / 100;
            uint32_t fraction = ((usartdiv % 100) * 16 + 50) / 100;
            brr_value = (mantissa << 4) | fraction;
            break;
    }

    if (is_initialized) {
        // Отключить USART перед изменением BRR
        USART1->CR1 &= ~USART_CR1_UE;
        USART1->BRR = brr_value;
        // Включить USART обратно
        USART1->CR1 |= USART_CR1_UE;
    } else {
        USART1->BRR = brr_value;
    }

    return UART_OK;
}

void uart_send_byte(uint8_t byte)
{
    // Ждем, пока передатчик не будет готов
    while ( !(USART1->SR & USART_SR_TXE) );

    USART1->DR = byte;
}

uart_error_t uart_send_data(const uint8_t* data, uint32_t size)
{
    if (!data || size == 0) {
        return UART_ERROR_PARAM;
    }

    // 2. Отправляем все байты
    for (uint32_t i = 0; i < size; i++)
    {
        uint32_t start_tick = get_tick_ms();
        
        // Ждем готовности передатчика (TXE)
        while (!(USART1->SR & USART_SR_TXE)) {
            if ((get_tick_ms() - start_tick) > TIMEOUT_2s) {
                return UART_ERROR_TIMEOUT;
            }
            
            // Проверка на ошибки (если нужно)
            if (USART1->SR & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) {
                // Сброс ошибок чтением SR
                uint32_t temp = USART1->SR;
                (void)temp;
                return UART_ERROR_PARAM;
            }
        }
        
        USART1->DR = data[i];
    }
    
    // 3. Ждем завершения передачи последнего байта (TC)
    uint32_t start_tick = get_tick_ms();
    while (!(USART1->SR & USART_SR_TC)) {
        if ((get_tick_ms() - start_tick) > TIMEOUT_2s) {
            return UART_ERROR_TIMEOUT;
        }
    }

    return UART_OK;
}

uart_error_t uart_send_string(const char* str)
{
    if (!str) {
        return UART_ERROR_PARAM;
    }

    return uart_send_data((const uint8_t*)str, strlen(str));
}

uart_error_t uart_send_line(const char* str)
{
    if (!str) {
        return UART_ERROR_PARAM;
    }

    uart_error_t result;
    
    // Отправляем основную строку
    result = uart_send_data((const uint8_t*)str, strlen(str));
    if (result != UART_OK) {
        return result;
    }
    
    // Отправляем перевод строки (CR+LF)
    result = uart_send_data((const uint8_t*)"\r\n", 2);
    
    return result;
}

uart_error_t uart_printf(const char* format, ...)
{
    va_list args;
    int length;
    
    // Форматируем строку во временный буфер
    va_start(args, format);
    length = vsnprintf(print_buffer, sizeof(print_buffer), format, args);
    va_end(args);
    
    // Проверяем на ошибки форматирования
    if (length < 0) {
        return UART_ERROR_PARAM;
    }
    
    // Если строка слишком длинная, обрезаем
    if (length >= sizeof(print_buffer)) {
        print_buffer[sizeof(print_buffer) - 1] = '\0';
    }
    
    // Отправляем через UART
    return uart_send_string(print_buffer);
}

uart_error_t uart_printf_line(const char* format, ...)
{
    va_list args;
    int length;

    // Форматируем строку во временный буфер
    va_start(args, format);
    length = vsnprintf(print_buffer, sizeof(print_buffer), format, args);
    va_end(args);
    
    // Проверяем на ошибки форматирования
    if (length < 0) {
        return UART_ERROR_PARAM;
    }
    
    // Если строка слишком длинная, обрезаем
    if (length >= sizeof(print_buffer)) {
        print_buffer[sizeof(print_buffer) - 1] = '\0';
    }
    
    // Отправляем строку с переводом строки
    return uart_send_line(print_buffer);
}

bool uart_is_ready_to_send(void)
{
    return (USART1->SR & USART_SR_TXE) != 0;
}

uint8_t uart_receive_byte(void)
{
    while ( !(USART1->SR & USART_SR_RXNE) );

    return (uint8_t)(USART1->DR & 0xFF);
}

bool uart_is_data_received(void)
{
    return (USART1->SR & USART_SR_RXNE) != 0;
}

// Функция для получения текущей скорости
uart_baudrate_t uart_get_current_baudrate(void)
{
    return current_baudrate;
}

// Обработчик прерывания USART1
void USART1_IRQHandler(void)
{
    if ( 0 != (USART1->SR & USART_SR_RXNE) )
    {
        uint32_t basepri = critical_enter_basp(); // Используем BASEPRI для быстрой защиты

        uint8_t received_data = (uint8_t) (USART1->DR & (uint8_t)0xFF);

        switch (received_data) {
            case '0':
                command_id = TURN_ALL_LEDS_OFF;
                break;
            case '1':
                command_id = TURN_LED_1_ON;
                break;
            case '2':
                command_id = TURN_LED_2_ON;
                break;
            case '3':
                command_id = TURN_LED_3_ON;
                break;
            case '4':
                command_id = ERASE_EEPROM;
                break;
            default:
                command_id = CMD_NONE;
                break;
        }

        critical_exit_basp(basepri);
    }

    // Обработка ошибок UART
    if (USART1->SR & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) {
        // Сброс ошибок
        volatile uint32_t temp = USART1->SR;
        temp = USART1->DR;
        (void)temp;
    }
}

command_id_t get_command_id(void)
{
    command_id_t cmd;
    
    CRITICAL_SECTION_START();
    cmd = command_id;
    command_id = CMD_NONE;
    CRITICAL_SECTION_END();
    
    return cmd;
}