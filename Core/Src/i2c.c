#include <stdint.h>
#include "stm32f407xx.h"
#include "i2c.h"

// Частота APB1 для USART1 (42 МГц после настройки RCC)
#define APB1_FREQUENCY 42000_000UL

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

    // 3. Отключить I2C для настройки
    I2C1->CR1 &= ~(I2C_CR1_PE);

    // 4. Настройка частоты (APB1 = 42 МГц)
    uint32_t number = (APB1_FREQUENCY / 1000_000UL);
    I2C1->CR2 |= (number << I2C_CR2_FREQ_Pos);               // FREQ = 42 МГц (для расчета таймингов)

    // 5. Настройка скорости
    i2c_error_t result = i2c_set_speed(config->clock_speed);
    if (result != I2C_OK) {
        return;
    }

    // 6. Настройка собственного адреса
    i2c_set_own_address(config->own_address);

    // 7. Настройка ACK
    if (config->enable_ack) {
        I2C1->CR1 |= I2C_CR1_ACK;
    } else {
        I2C1->CR1 &= ~I2C_CR1_ACK;
    }

    // 8. Настройка общего вызова
    if (config->enable_general_call) {
        I2C1->CR1 |= I2C_CR1_ENGC;
    } else {
        I2C1->CR1 &= ~I2C_CR1_ENGC;
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
    if (!is_initialized) {
        return I2C_ERROR_BUSY;
    }
    
    // Отключить I2C перед изменением
    I2C1->CR1 &= ~I2C_CR1_PE;
    
    // Установить адрес (7-битный режим)
    I2C1->OAR1 = (address << 1) | I2C_OAR1_ADDMODE;
    
    // Включить I2C обратно
    I2C1->CR1 |= I2C_CR1_PE;
    
    return I2C_OK;
}

void i2c_generate_start()
{
    I2C1->CR1 |= I2C_CR1_START;                     // команда на формировать START условия
    while ( (I2C1->SR1 & I2C_SR1_SB) == 0 );        // ожидание, что START-условие было сформировано на шине I2C
}

static void i2c_generate_stop(void)
{
    I2C1->CR1 |= I2C_CR1_STOP;
    current_state = I2C_STATE_IDLE;
}

i2c_error_t i2c_master_transmit(uint8_t slave_addr, const uint8_t *data,  uint32_t size, uint32_t timeout_ms)
{
    if (!is_initialized || !data || 0 == size) {
        return I2C_ERROR_PARAM;
    }
    
    if (current_state != I2C_STATE_IDLE) {
        return I2C_ERROR_BUSY;
    }
    
    uint32_t timeout = timeout_ms;
    if (timeout == 0) {
        timeout = I2C_TIMEOUT_DEFAULT_MS;
    }
    
    current_state = I2C_STATE_MASTER_TX;
    last_error = I2C_OK;

    // 1. Генерируем START условие
    I2C1->CR1 |= I2C_CR1_START;

    // 2. Ждем подтверждения START (SB флаг)
    uint32_t wait_start = timeout * 1000;
    while ( !(I2C1->SR1 & I2C_SR1_SB) ) {
        if (--wait_start == 0) {
            current_state = I2C_STATE_IDLE;
            return I2C_ERROR_TIMEOUT;
        }
        if (I2C1->SR1 & I2C_SR1_ARLO) {
            I2C1->SR1 &= ~I2C_SR1_ARLO;
            current_state = I2C_STATE_IDLE;
            return I2C_ERROR_ARBITRATION_LOST;
        }
    }

    // 3. Отправляем адрес (7 бит + бит записи = 0)
    I2C1->DR = (slave_addr << 1) | 0;

    // 4. Ждем подтверждения адреса (ADDR флаг)
    uint32_t wait_addr = timeout * 1000;
    while ( !(I2C1->SR1 & I2C_SR1_ADDR) ) {
        if (--wait_addr == 0) {
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

    // 5. Очищаем ADDR флаг чтением SR1 и SR2
    volatile uint32_t tmp = I2C1->SR1;
    tmp = I2C1->SR2;
    (void) tmp;

    // 6. Отправляем данные
    for (uint32_t i = 0; i < size; i++) {
        // Ждем, пока TXE не станет пустым
        uint32_t wait_txe = timeout * 1000;
        while ( !(I2C1->SR1 & I2C_SR1_TXE) ) {
            if (--wait_txe == 0) {
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
        
        I2C1->DR = data[i];
    }

    // 7. Ждем завершения передачи (BTF флаг)
    uint32_t wait_btf = timeout * 1000;
    while (!(I2C1->SR1 & I2C_SR1_BTF)) {
        if (--wait_btf == 0) {
            i2c_generate_stop();
            current_state = I2C_STATE_IDLE;
            return I2C_ERROR_TIMEOUT;
        }
    }

    // 8. Генерируем STOP условие
    i2c_generate_stop();
    
    current_state = I2C_STATE_IDLE;
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
    
    uint32_t timeout = timeout_ms;
    if (timeout == 0) {
        timeout = I2C_TIMEOUT_DEFAULT_MS;
    }
    
    current_state = I2C_STATE_MASTER_RX;
    last_error = I2C_OK;
    
    // 1. Генерируем START условие
    I2C1->CR1 |= I2C_CR1_START;
    
    // 2. Ждем подтверждения START (SB флаг)
    uint32_t wait_start = timeout * 1000;
    while (!(I2C1->SR1 & I2C_SR1_SB)) {
        if (--wait_start == 0) {
            current_state = I2C_STATE_IDLE;
            return I2C_ERROR_TIMEOUT;
        }
    }
    
    // 3. Отправляем адрес (7 бит + бит чтения = 1)
    I2C1->DR = (slave_addr << 1) | 1;
    
    // 4. Ждем подтверждения адреса (ADDR флаг)
    uint32_t wait_addr = timeout * 1000;
    while ( !(I2C1->SR1 & I2C_SR1_ADDR) ) {
        if (--wait_addr == 0) {
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
    
    // Для приема одного байта нужно отключить ACK перед очисткой ADDR
    if (size == 1) {
        I2C1->CR1 &= ~I2C_CR1_ACK;
    }
    
    // 5. Очищаем ADDR флаг
    volatile uint32_t tmp = I2C1->SR1;
    tmp = I2C1->SR2;
    (void)tmp;
    
    // 6. Если принимаем только один байт, генерируем STOP сразу после ADDR
    if (size == 1) {
        i2c_generate_stop();
    }
    
    // 7. Принимаем данные
    for (uint32_t i = 0; i < size; i++) {
        // Для предпоследнего байта отключаем ACK
        if (size > 1 && i == (size - 2)) {
            I2C1->CR1 &= ~I2C_CR1_ACK;
        }
        
        // Ждем, пока RXNE не станет полным
        uint32_t wait_rxne = timeout * 1000;
        while ( !(I2C1->SR1 & I2C_SR1_RXNE) ) {
            if (--wait_rxne == 0) {
                i2c_generate_stop();
                current_state = I2C_STATE_IDLE;
                return I2C_ERROR_TIMEOUT;
            }
        }
        
        buffer[i] = I2C1->DR;
        
        // Для последнего байта генерируем STOP (если еще не сгенерировали)
        if ( size > 1 && i == (size - 1) ) {
            i2c_generate_stop();
        }
    }
    
    // Восстанавливаем ACK для будущих транзакций
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
