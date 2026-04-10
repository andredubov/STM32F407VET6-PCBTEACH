#include <stdint.h>
#include "stm32f407xx.h"
#include "i2c.h"
#include "delay.h"

// Частота APB1 для USART1 (42 МГц после настройки RCC)
#define APB1_FREQUENCY 42000000UL

// Таймауты по умолчанию (в мкс)
#define I2C_TIMEOUT_DEFAULT_MS 1000

// Статусы состояния I2C
typedef enum {
    I2C_STATE_IDLE = 0,
    I2C_STATE_MASTER_TX,
    I2C_STATE_MASTER_RX,
    I2C_STATE_SLAVE_TX,
    I2C_STATE_SLAVE_RX
} i2c_state_t;

// Глобальные переменные
static volatile bool is_initialized = false;
static volatile i2c_state_t current_state = I2C_STATE_IDLE;
static volatile i2c_error_t last_error = I2C_OK;

// Конфигурация по умолчанию
static i2c_config_t default_config = {
    .clock_speed = I2C_MODE_STANDARD,
    .own_address = 0x30,
    .enable_ack = true,
    .enable_general_call = false
};

void i2c_init_with_config(const i2c_config_t *config)
{
    if (!config) {
        config = &default_config;
    }

    if (is_initialized) {
        return;
    }

    // 1. Включить тактирование GPIOB и I2C1
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;     // включение тактирование модуля I2C1
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;    // включение тактирование порта GPIOB (PB8 = SCL, PB9 = SDA)

    // 2. Настроить пины PB8 (SCL) и PB9 (SDA) в режим Alternate Function
    GPIOB->MODER |= GPIO_MODER_MODER8_1;    // режим альтернативной функции
    GPIOB->MODER |= GPIO_MODER_MODER9_1;    // режим альтернативной функции

    GPIOB->AFR[1] |= GPIO_AFRH_AFRH0_2;     // для PB8 выбрана альтернативная функция AF4 = I2C1
    GPIOB->AFR[1] |= GPIO_AFRH_AFRH1_2;     // для PB9 выбрана альтернативная функция AF4 = I2C1

    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD8 | GPIO_PUPDR_PUPDR9); // отключение подтягивающих резисторов
    GPIOB->OTYPER |= (GPIO_OTYPER_OT8 | GPIO_OTYPER_OT9);    // включение режима Open Drain

    // 3. Отключить I2C для настройки
    I2C1->CR1 &= ~(I2C_CR1_PE);

    // 4. Настройка частоты (APB1 = 42 МГц)
    uint32_t number = (APB1_FREQUENCY / 1000000UL);
    I2C1->CR2 |= (number << I2C_CR2_FREQ_Pos);               // FREQ = 42 МГц (для расчета таймингов)

    // 5. Настройка скорости
    i2c_error_t result = i2c_set_speed(config->clock_speed);
    if (result != I2C_OK) {
        return;
    }

    // 6. Настройка собственного адреса
    i2c_set_own_address(config->own_address);

    // 7. Настройка общего вызова
    if (config->enable_general_call) {
        I2C1->CR1 |= I2C_CR1_ENGC;
    } else {
        I2C1->CR1 &= ~(I2C_CR1_ENGC);
    }

    // 8. Настройка ACK
    if (config->enable_ack) {
        I2C1->CR1 |= I2C_CR1_ACK;
    } else {
        I2C1->CR1 &= ~(I2C_CR1_ACK);
    }

    // 9. Включить I2C
    I2C1->CR1 |= I2C_CR1_PE;

    is_initialized = true;
}

void i2c_init(void)
{
    i2c_init_with_config(&default_config);
}

i2c_error_t i2c_set_own_address(uint8_t address)
{
    // Отключить I2C перед изменением
    I2C1->CR1 &= ~(I2C_CR1_PE);
    
    // Установить адрес (7-битный режим)
    I2C1->OAR1 = (address << 1) | I2C_OAR1_ADDMODE;
    
    // Включить I2C обратно
    I2C1->CR1 |= I2C_CR1_PE;
    
    return I2C_OK;
}

i2c_error_t i2c_set_speed(i2c_speed_t speed)
{
    uint32_t ccr_value = 0;
    
    if (speed == I2C_MODE_STANDARD)  // Расчет CCR для Standard mode (100 кГц)
    {
        ccr_value = APB1_FREQUENCY / (2 * 100000);
        if (ccr_value < 4) {
            ccr_value = 4;  // Минимальное значение
        }
        I2C1->CCR &= ~(I2C_CCR_FS);
        I2C1->CCR |= ccr_value;
        ccr_value = (APB1_FREQUENCY / 1000000) + 1;
        I2C1->TRISE |= (ccr_value << I2C_TRISE_TRISE_Pos);
    }
    else if (speed == I2C_MODE_FAST)  // Расчет CCR для Fast mode (400 кГц)
    {
        ccr_value = APB1_FREQUENCY / (3 * 400000);
        if (ccr_value < 4) {
            ccr_value = 4;  // Минимальное значение
        }
        I2C1->CCR |= ccr_value | I2C_CCR_FS;
        ccr_value = (APB1_FREQUENCY / 1000000) * 300 / 1000 + 1;
        I2C1->TRISE |= (ccr_value << I2C_TRISE_TRISE_Pos);
    }
    else
    {
        return I2C_ERROR_PARAM;
    }
    
    return I2C_OK;
}

static void i2c_generate_stop(void)
{
    I2C1->CR1 |= I2C_CR1_STOP;
    current_state = I2C_STATE_IDLE;
}

static i2c_error_t i2c_generate_start(uint32_t timeout_ms)
{
    uint32_t current_tick = 0;
    uint32_t start_tick = 0;
    uint32_t timeout = timeout_ms;

    if (0 == timeout) {
        timeout = I2C_TIMEOUT_DEFAULT_MS;
    }

    // 1. Проверяем что шина свободна
    start_tick = get_tick_ms();
    while (I2C1->SR2 & I2C_SR2_BUSY) {
        current_tick = get_tick_ms();
        if ((current_tick - start_tick) > timeout) {
            current_state = I2C_STATE_IDLE;
            return I2C_ERROR_BUSY;
        }
    }

    // 2. Генерируем START условие
    I2C1->CR1 |= I2C_CR1_START;

    // 3. Ждем подтверждения START (SB флаг)
    start_tick = get_tick_ms();
    while (0 == (I2C1->SR1 & I2C_SR1_SB)) {
        current_tick = get_tick_ms();
        if ((current_tick - start_tick) > timeout) {
            current_state = I2C_STATE_IDLE;
            return I2C_ERROR_TIMEOUT;
        }

        // Проверка ошибок
        if (I2C1->SR1 & I2C_SR1_ARLO) {
            // Сброс ARLO корректным способом
            volatile uint32_t temp = I2C1->SR1;
            temp = I2C1->SR2;
            (void)temp;  // подавляем warning            
            current_state = I2C_STATE_IDLE;
            return I2C_ERROR_ARBITRATION_LOST;
        }

        if (I2C1->SR1 & I2C_SR1_AF) {
            I2C1->SR1 &= ~I2C_SR1_AF;
            i2c_generate_stop();
            delay_ms(1);
            return I2C_ERROR_ACK_FAILURE;
        }
    }

    return I2C_OK;
}

static i2c_error_t i2c_send_address(uint8_t slave_addr, i2c_rw_bit_t rw_bit, uint32_t timeout_ms)
{
    uint32_t current_tick = 0;
    uint32_t start_tick = 0;
    uint32_t timeout = timeout_ms;

    if (0 == timeout) {
        timeout = I2C_TIMEOUT_DEFAULT_MS;
    }

    // 1. Отправка адресса
    switch (rw_bit)
    {
        case i2c_write_bit:
            slave_addr &= ~(1 << 0);
            break;
        case i2c_read_bit:
            slave_addr |= (1 << 0);
            break;
        default:
            break;
    }

    I2C1->DR = slave_addr;

    // 2. Ждем подтверждения адреса (ADDR флаг)
    start_tick = get_tick_ms();
    while ( (I2C1->SR1 & I2C_SR1_ADDR) == 0 ) {
        current_tick = get_tick_ms();
        if ((current_tick - start_tick) > timeout) {
            i2c_generate_stop();
            delay_ms(1);
            return I2C_ERROR_TIMEOUT;
        }

        if (I2C1->SR1 & I2C_SR1_AF) {
            I2C1->SR1 &= ~I2C_SR1_AF;
            i2c_generate_stop();
            delay_ms(1);
            return I2C_ERROR_ACK_FAILURE;
        }
    }

    // 3. Очищаем ADDR флаг чтением SR1 и SR2
    volatile uint32_t tmp = I2C1->SR1;
    tmp = I2C1->SR2;
    (void) tmp;

    return I2C_OK;
}

i2c_error_t i2c_master_transmit(uint8_t slave_addr, const uint8_t *data,  uint32_t size, uint32_t timeout_ms)
{
    if (!is_initialized || !data || 0 == size) {
        return I2C_ERROR_PARAM;
    }

    if (current_state != I2C_STATE_IDLE) {
        return I2C_ERROR_BUSY;
    }

    uint32_t current_tick = 0;
    uint32_t start_tick = 0;
    uint32_t timeout = timeout_ms;

    if (0 == timeout) {
        timeout = I2C_TIMEOUT_DEFAULT_MS;
    }

    current_state = I2C_STATE_MASTER_TX;
    last_error = I2C_OK;

    // 1. Включить генерацию битов ACK
    I2C1->CR1 |= I2C_CR1_ACK;

    i2c_error_t error = i2c_generate_start(timeout_ms);
    if (I2C_OK != error) {
        return error;
    }

    error = i2c_send_address(slave_addr, i2c_write_bit, timeout_ms);
    if (I2C_OK != error) {
        return error;
    }

    // 8. Отправляем данные
    for (uint32_t i = 0; i < size; i++) {
        start_tick = get_tick_ms();
        // Ждем, пока TXE не станет пустым
        while ( !(I2C1->SR1 & I2C_SR1_TXE) ) {
            current_tick = get_tick_ms();

            if ((current_tick - start_tick) > timeout) {
                i2c_generate_stop();
                return I2C_ERROR_TIMEOUT;
            }

            if (I2C1->SR1 & I2C_SR1_AF) {
                I2C1->SR1 &= ~I2C_SR1_AF;
                i2c_generate_stop();
                delay_ms(1);
                return I2C_ERROR_ACK_FAILURE;
            }
        }

        I2C1->DR = data[i];
    }

    // 9. Ждем завершения передачи (BTF флаг)
    start_tick = get_tick_ms();
    while (0 == (I2C1->SR1 & I2C_SR1_BTF)) {
        current_tick = get_tick_ms();
        if ((current_tick - start_tick) > timeout) {
            i2c_generate_stop();
            return I2C_ERROR_TIMEOUT;
        }
    }

    // 10. Генерируем STOP условие
    i2c_generate_stop();

    return I2C_OK;
}

i2c_error_t i2c_master_receive(uint8_t slave_addr, uint8_t *buffer, uint32_t size, uint32_t timeout_ms)
{
    if (!is_initialized || !buffer || size == 0) {
        return I2C_ERROR_PARAM;
    }
    
    if (current_state != I2C_STATE_IDLE) {
        return I2C_ERROR_BUSY;
    }
    
    uint32_t timeout = (timeout_ms == 0) ? I2C_TIMEOUT_DEFAULT_MS : timeout_ms;
    current_state = I2C_STATE_MASTER_RX;
    
    // START + адрес (общий код)
    i2c_error_t error = i2c_generate_start(timeout_ms);
    if (error != I2C_OK) {
        current_state = I2C_STATE_IDLE;
        return error;
    }
    
    I2C1->DR = (slave_addr | 0x01);
    
    // Ждем ADDR
    uint32_t start_tick = get_tick_ms();
    while ((I2C1->SR1 & I2C_SR1_ADDR) == 0) 
    {
        if ((get_tick_ms() - start_tick) > timeout) {
            i2c_generate_stop();
            current_state = I2C_STATE_IDLE;
            return I2C_ERROR_TIMEOUT;
        }
        
        if (I2C1->SR1 & I2C_SR1_AF) {
            I2C1->SR1 &= ~I2C_SR1_AF;
            i2c_generate_stop();
            current_state = I2C_STATE_IDLE;
            return I2C_ERROR_ACK_FAILURE;
        }
    }
    
    // Особый случай: прием 1 байта
    if (size == 1) {
        // Отключаем ACK
        I2C1->CR1 &= ~I2C_CR1_ACK;
        
        // Очищаем ADDR
        volatile uint32_t tmp = I2C1->SR1;
        tmp = I2C1->SR2;
        (void)tmp;
        
        // Генерируем STOP
        i2c_generate_stop();
        
        // Ждем единственный байт
        start_tick = get_tick_ms();
        while ((I2C1->SR1 & I2C_SR1_RXNE) == 0) 
        {
            if ((get_tick_ms() - start_tick) > timeout) {
                current_state = I2C_STATE_IDLE;
                return I2C_ERROR_TIMEOUT;
            }
        }
        
        buffer[0] = I2C1->DR;
        
        // Восстанавливаем ACK
        I2C1->CR1 |= I2C_CR1_ACK;
        current_state = I2C_STATE_IDLE;
        return I2C_OK;
    }
    
    // Прием 2 и более байтов
    // Очищаем ADDR (ACK еще включен)
    volatile uint32_t tmp = I2C1->SR1;
    tmp = I2C1->SR2;
    (void)tmp;
    
    // Принимаем все байты, кроме последнего
    for (uint32_t i = 0; i < size - 1; i++) {
        // Ждем данные
        start_tick = get_tick_ms();
        while ((I2C1->SR1 & I2C_SR1_RXNE) == 0) 
        {
            if ((get_tick_ms() - start_tick) > timeout) {
                i2c_generate_stop();
                current_state = I2C_STATE_IDLE;
                return I2C_ERROR_TIMEOUT;
            }
        }
        
        buffer[i] = I2C1->DR;
    }
    
    // Последний байт: отключаем ACK и генерируем STOP
    I2C1->CR1 &= ~I2C_CR1_ACK;
    
    // Ждем последний байт
    start_tick = get_tick_ms();
    while ((I2C1->SR1 & I2C_SR1_RXNE) == 0) {
        if ((get_tick_ms() - start_tick) > timeout) {
            i2c_generate_stop();
            current_state = I2C_STATE_IDLE;
            return I2C_ERROR_TIMEOUT;
        }
    }
    
    buffer[size - 1] = I2C1->DR;
    i2c_generate_stop();  // STOP после последнего байта
    
    // Восстанавливаем ACK
    I2C1->CR1 |= I2C_CR1_ACK;
    
    current_state = I2C_STATE_IDLE;
    return I2C_OK;
}

i2c_error_t i2c_master_write_then_read(
    uint8_t slave_addr,
    const uint8_t *write_data,
    uint32_t write_size,
    uint8_t *read_buffer,
    uint32_t read_size,
    uint32_t timeout_ms)
{
    i2c_error_t result;
    
    // Сначала записываем
    result = i2c_master_transmit(slave_addr, write_data, write_size, timeout_ms);
    if (result != I2C_OK) {
        return result;
    }
    
    // Небольшая задержка для некоторых устройств
    delay_ms(1);
    
    // Затем читаем
    result = i2c_master_receive(slave_addr, read_buffer, read_size, timeout_ms);
    
    return result;
}

const char* i2c_get_error_string(i2c_error_t error)
{
    switch (error) {
        case I2C_OK: return "OK";
        case I2C_ERROR_BUSY: return "Busy";
        case I2C_ERROR_TIMEOUT: return "Timeout";
        case I2C_ERROR_ARBITRATION_LOST: return "Arbitration lost";
        case I2C_ERROR_ACK_FAILURE: return "ACK failure";
        case I2C_ERROR_BUS_ERROR: return "Bus error";
        case I2C_ERROR_PARAM: return "Invalid parameter";
        default: 
            return "Unknown error";
    }
}