#include "stm32f407xx.h"
#include "uart.h"

// Частота APB2 для USART1 (42 МГц после настройки RCC)
#define APB2_FREQUENCY 42000000UL

// Внутренние переменные
static bool is_initialized = false;
static uart_baudrate_t current_baudrate = UART_BAUDRATE_115200;
volatile static command_id_t command_id = TURN_ALL_LEDS_OFF;

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

    // 3. Настроить USART
    USART1->CR1 &= ~(USART_CR1_UE);  // Отключить USART для настройки

    // Установка скорости по умолчанию
    uart_error_t result = uart_set_baudrate(current_baudrate);
    if (UART_ERROR_PARAM == result) {
        return;
    }

    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;     // Вкл. передатчик и приемник
    USART1->CR1 &= ~(USART_CR1_M | USART_CR1_PCE);  // 8 бит, без контроля четности
    USART1->CR2 &= ~(USART_CR2_STOP);               // 1 стоповый бит

    // 4. Включить прерывания (если нужно)
    USART1->CR1 |= USART_CR1_RXNEIE;                // Прерывание по приему
    NVIC_EnableIRQ(USART1_IRQn);
    NVIC_SetPriority(USART1_IRQn, 0);

    // 5. Включить USART
    USART1->CR1 |= USART_CR1_UE;
    
    is_initialized = true;
}

uart_error_t uart_set_baudrate(uart_baudrate_t baudrate)
{
    if (baudrate < 9600 || baudrate > 3000000) {
        return UART_ERROR_PARAM;
    }

    // Сохранить текущую скорость
    current_baudrate = baudrate;

    if (is_initialized) {
        // Отключить USART перед изменением BRR
        USART1->CR1 &= ~USART_CR1_UE;
        // Рассчитать BRR (округление к ближайшему)
        uint32_t brr_value = (APB2_FREQUENCY + baudrate/2) / baudrate;
        USART1->BRR = brr_value;
        // Включить USART обратно
        USART1->CR1 |= USART_CR1_UE;
    } else {
        // Рассчитать BRR (округление к ближайшему)
        uint32_t brr_value = (APB2_FREQUENCY + baudrate/2) / baudrate;
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

    for (uint32_t i = 0; i < size; i++) 
    {
        // Проверка таймаута (защита от зависания)
        uint32_t timeout = 1000000;
        while ( !(USART1->SR & USART_SR_TXE) )
        {
            if (--timeout == 0) {
                return UART_ERROR_TIMEOUT;
            }
        }
        USART1->DR = data[i];
    }

    // Ждем завершения передачи последнего байта
    uint32_t timeout = 1000000;
    while ( !(USART1->SR & USART_SR_TC) ) 
    {
        if (--timeout == 0) {
            return UART_ERROR_TIMEOUT;
        }
    }

    return UART_OK;
}

bool uart_is_ready_to_send(void)
{
    return (USART1->SR & USART_SR_TXE) != 0;
}

uint8_t uart_receive_byte(void)
{
    while (!(USART1->SR & USART_SR_RXNE));

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
    if (USART1->SR & USART_SR_RXNE) 
    {
        uint8_t received_data = (uint8_t) (USART1->DR & 0xFF);

        switch (received_data) {
            case 1:
                command_id = TURN_LED_1_ON;
                break;
            case 2:
                command_id = TURN_LED_2_ON;
                break;
            case 3:
                command_id = TURN_LED_3_ON;
                break;
            default:
                command_id = TURN_ALL_LEDS_OFF;
                break;
        }
    }
}

command_id_t get_command_id(void)
{
    return command_id;
}