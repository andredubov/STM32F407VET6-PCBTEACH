#include "stm32f407xx.h"
#include "spi.h"
#include "delay.h"

// Частота APB1 для SPI2 (42 МГц после настройки RCC)
#define APB1_FREQUENCY 42000000UL
#define SPI_TIMEOUT_MS 1000

// Статическая конфигурация
static bool is_initialized = false;

static spi_config_t default_config = {
    .mode = SPI_MODE_0,
    .data_size = SPI_DATA_SIZE_8BIT,
    .baudrate = SPI_BAUDRATE_DIV_32,
    .msb_first = true,
    .software_ssm = true
};

static spi_config_t current_config = {
    .mode = SPI_MODE_0,
    .data_size = SPI_DATA_SIZE_8BIT,
    .baudrate = SPI_BAUDRATE_DIV_32,
    .msb_first = true,
    .software_ssm = true
};

// Инициализация SPI2 с конфигурацией по умолчанию
void spi_init(void)
{
    spi_init_with_config(&default_config);
}

// Инициализация SPI2 с пользовательской конфигурацией
void spi_init_with_config(const spi_config_t *config)
{
    if (is_initialized) {
        return;
    }

    if (config) {
        current_config = *config;
    }

    // 1. Включить тактирование GPIOB, GPIOC, GPIOE и SPI2
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    // 2. Настроить пины
    // PB10 - SCK (альтернативная функция AF5)
    GPIOB->MODER &= ~(GPIO_MODER_MODER10);
    GPIOB->MODER |= GPIO_MODER_MODER10_1;  // Альтернативная функция
    GPIOB->AFR[1] &= ~(GPIO_AFRH_AFSEL10);
    GPIOB->AFR[1] |= (5 << GPIO_AFRH_AFSEL10_Pos);  // AF5 для SPI2
    GPIOB->OTYPER &= ~(GPIO_OTYPER_OT_10);  // Push-pull
    GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR10;  // Высокая скорость
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPDR10);  // Без подтяжки

    // PC2 - MISO (альтернативная функция AF5)
    GPIOC->MODER &= ~(GPIO_MODER_MODER2);
    GPIOC->MODER |= GPIO_MODER_MODER2_1;  // Альтернативная функция
    GPIOC->AFR[0] &= ~(GPIO_AFRL_AFSEL2);
    GPIOC->AFR[0] |= (5 << GPIO_AFRL_AFSEL2_Pos);  // AF5 для SPI2
    GPIOC->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR2;  // Высокая скорость
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPDR2);  // Без подтяжки

    // PC3 - MOSI (альтернативная функция AF5)
    GPIOC->MODER &= ~(GPIO_MODER_MODER3);
    GPIOC->MODER |= GPIO_MODER_MODER3_1;  // Альтернативная функция
    GPIOC->AFR[0] &= ~(GPIO_AFRL_AFSEL3);
    GPIOC->AFR[0] |= (5 << GPIO_AFRL_AFSEL3_Pos);  // AF5 для SPI2
    GPIOC->OTYPER &= ~(GPIO_OTYPER_OT_3);  // Push-pull
    GPIOC->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR3;  // Высокая скорость
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPDR3);  // Без подтяжки

    // PE3 - CS (обычный выход)
    GPIOE->MODER &= ~(GPIO_MODER_MODER3);
    GPIOE->MODER |= GPIO_MODER_MODER3_0;  // Выход
    GPIOE->OTYPER &= ~(GPIO_OTYPER_OT_3);  // Push-pull
    GPIOE->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR3;  // Высокая скорость
    GPIOE->PUPDR &= ~(GPIO_PUPDR_PUPDR3);  // Без подтяжки
    GPIOE->BSRR |= GPIO_BSRR_BS3; // нективный уровень PE3 (CS=1)

    // 3. Сбросить SPI2 перед настройкой
    spi_disable();

    // 4. Настройка SPI2
    // CR1: CPOL, CPHA, BR, MSTR, LSBFIRST, DFF, SSM, SSI
    uint32_t cr1 = 0;

    // Режим ведущего
    cr1 |= SPI_CR1_MSTR;

    // Настройка режима SPI (CPOL, CPHA)
    if (current_config.mode & 0x02) {
        cr1 |= SPI_CR1_CPOL;  // CPOL = 1
    }
    if (current_config.mode & 0x01) {
        cr1 |= SPI_CR1_CPHA;  // CPHA = 1
    }

    // Размер данных
    if (current_config.data_size == SPI_DATA_SIZE_16BIT) {
        cr1 |= SPI_CR1_DFF;
    } else {
        cr1 &= ~(SPI_CR1_DFF);
    }
    
    // Порядок битов
    if (!current_config.msb_first) {
        cr1 |= SPI_CR1_LSBFIRST;
    } else {
        cr1 &= ~(SPI_CR1_LSBFIRST);
    }

    // Скорость (делитель)
    cr1 |= (current_config.baudrate << SPI_CR1_BR_Pos);

    // Программное управление CS
    if (current_config.software_ssm) {
        cr1 |= (SPI_CR1_SSM | SPI_CR1_SSI);
    }

    SPI2->CR1 = cr1;
    SPI2->CR2 = 0;

    // 5. Включить SPI
    spi_enable();

    // 6. Установить CS в неактивное состояние
    spi_cs_deselect();

    is_initialized = true;
}

// Установка новой конфигурации
spi_error_t spi_set_config(const spi_config_t *config)
{
    if (!config) {
        return SPI_ERROR_PARAM;
    }

    bool was_enabled = (SPI2->CR1 & SPI_CR1_SPE) != 0;

    if (was_enabled) {
        spi_disable();
    }

    current_config = *config;

    // Пересборка CR1
    uint32_t cr1 = 0;
    cr1 |= SPI_CR1_MSTR;

    if (current_config.mode & 0x02) {
        cr1 |= SPI_CR1_CPOL;
    }

    if (current_config.mode & 0x01) {
        cr1 |= SPI_CR1_CPHA;
    }

    if (current_config.data_size == SPI_DATA_SIZE_16BIT) {
        cr1 |= SPI_CR1_DFF;
    } else {
        cr1 &= ~(SPI_CR1_DFF);
    }

    if (!current_config.msb_first) {
        cr1 |= SPI_CR1_LSBFIRST;
    } else {
        cr1 &= ~(SPI_CR1_LSBFIRST);
    }

    if (current_config.software_ssm) {
        cr1 |= (SPI_CR1_SSM | SPI_CR1_SSI);
    }

    cr1 |= (current_config.baudrate << SPI_CR1_BR_Pos);

    SPI2->CR1 = cr1;

    if (was_enabled) {
        spi_enable();
    }

    return SPI_OK;
}

// Программное управление CS (активный низкий уровень)
void spi_cs_select(void)
{
    GPIOE->BSRR = GPIO_BSRR_BR3;  // Сброс PE3 (активный низкий уровень)
}

void spi_cs_deselect(void)
{
    GPIOE->BSRR = GPIO_BSRR_BS3;  // Установка PE3 (неактивное состояние)
}

void spi_cs_set(bool select)
{
    if (select) {
        spi_cs_select();
    } else {
        spi_cs_deselect();
    }
}

// Включение/выключение SPI
void spi_enable(void)
{
    SPI2->CR1 |= SPI_CR1_SPE;
}

void spi_disable(void)
{
    SPI2->CR1 &= ~SPI_CR1_SPE;
}

// Проверка занятости
bool spi_is_busy(void)
{
    return (SPI2->SR & SPI_SR_BSY) != 0;
}

// Очистка RX буфера
void spi_flush_rx(void)
{
    while (SPI2->SR & SPI_SR_RXNE) {
        (void)SPI2->DR;
    }
}

// Отправка одного байта
spi_error_t spi_transmit_byte(uint8_t data)
{
    uint32_t start_tick = get_tick_ms();
    
    // Ждем, пока TX буфер не опустеет
    while ( !(SPI2->SR & SPI_SR_TXE) ) {
        if ((get_tick_ms() - start_tick) > SPI_TIMEOUT_MS) {
            return SPI_ERROR_TIMEOUT;
        }
    }
    
    // Отправляем данные
    SPI2->DR = data;
    
    start_tick = get_tick_ms();
    while ( !(SPI2->SR & SPI_SR_RXNE) ) {
        if ((get_tick_ms() - start_tick) > SPI_TIMEOUT_MS) {
            return SPI_ERROR_TIMEOUT;
        }

        // Проверка на переполнение
        if (SPI2->SR & SPI_SR_OVR) {
            // Сброс флага OVR чтением SR и DR
            volatile uint32_t temp = SPI2->SR;
            temp = SPI2->DR;
            (void)temp;
            return SPI_ERROR_OVERRUN;
        }
    }

    data = (uint8_t)SPI2->DR;

    // Ждем завершения передачи (BSY)
    start_tick = get_tick_ms();
    while ( SPI2->SR & SPI_SR_BSY ) {
        if ((get_tick_ms() - start_tick) > SPI_TIMEOUT_MS) {
            return SPI_ERROR_TIMEOUT;
        }
    }
    
    return SPI_OK;
}

// Прием одного байта
spi_error_t spi_receive_byte(uint8_t *data)
{
    if (!data) {
        return SPI_ERROR_PARAM;
    }

    // 1. Ждем готовности TX буфера
    uint32_t start_tick = get_tick_ms();
    while ( !(SPI2->SR & SPI_SR_TXE) ) {
        if ((get_tick_ms() - start_tick) > SPI_TIMEOUT_MS) {
            return SPI_ERROR_TIMEOUT;
        }
    }

    // 2. Отправляем dummy байт для генерации тактов
    SPI2->DR = 0xFF;

    // 3. Ждем приема данных
    start_tick = get_tick_ms();
    while ( !(SPI2->SR & SPI_SR_RXNE) ) {
        if ((get_tick_ms() - start_tick) > SPI_TIMEOUT_MS) {
            return SPI_ERROR_TIMEOUT;
        }

        // Проверка на переполнение
        if (SPI2->SR & SPI_SR_OVR) {
            // Сброс флага OVR чтением SR и DR
            volatile uint32_t temp = SPI2->SR;
            temp = SPI2->DR;
            (void)temp;
            return SPI_ERROR_OVERRUN;
        }
    }

    // 4. Читаем данные
    *data = (uint8_t)SPI2->DR;

    // Ждем завершения передачи (BSY)
    start_tick = get_tick_ms();
    while ( SPI2->SR & SPI_SR_BSY ) {
        if ((get_tick_ms() - start_tick) > SPI_TIMEOUT_MS) {
            return SPI_ERROR_TIMEOUT;
        }
    }

    return SPI_OK;
}

// Одновременная передача и прием
spi_error_t spi_transmit_receive_byte(uint8_t tx_data, uint8_t *rx_data)
{
    if (!rx_data) {
        return SPI_ERROR_PARAM;
    }
    
    // Ждем готовности TX
    uint32_t start_tick = get_tick_ms();
    while ( 0 == (SPI2->SR & SPI_SR_TXE) ) {
        if ((get_tick_ms() - start_tick) > SPI_TIMEOUT_MS) {
            return SPI_ERROR_TIMEOUT;
        }
    }
    
    // Отправляем данные
    SPI2->DR = tx_data;
    
    // Ждем, пока RX буфер не заполнится
    start_tick = get_tick_ms();
    while ( 0 == (SPI2->SR & SPI_SR_RXNE) ) {
        if ((get_tick_ms() - start_tick) > SPI_TIMEOUT_MS) {
            return SPI_ERROR_TIMEOUT;
        }
        
        if (SPI2->SR & SPI_SR_OVR) {
            volatile uint32_t temp = SPI2->SR;
            temp = SPI2->DR;
            (void)temp;
            return SPI_ERROR_OVERRUN;
        }
    }
    
    // Читаем принятые данные
    *rx_data = (uint8_t)SPI2->DR;
    
    // Ждем завершения передачи
    start_tick = get_tick_ms();
    while ( SPI2->SR & SPI_SR_BSY ) {
        if ((get_tick_ms() - start_tick) > SPI_TIMEOUT_MS) {
            return SPI_ERROR_TIMEOUT;
        }
    }
    
    return SPI_OK;
}

// Отправка буфера
spi_error_t spi_transmit_buffer(const uint8_t *tx_buffer, uint32_t size)
{
    if (!tx_buffer || size == 0) {
        return SPI_ERROR_PARAM;
    }
    
    for (uint32_t i = 0; i < size; i++) {
        spi_error_t spi_error = spi_transmit_byte(tx_buffer[i]);
        if (spi_error != SPI_OK) {
            return spi_error;
        }
    }
    
    return SPI_OK;
}

// Прием буфера
spi_error_t spi_receive_buffer(uint8_t *rx_buffer, uint32_t size)
{
    if (!rx_buffer || size == 0) {
        return SPI_ERROR_PARAM;
    }
    
    for (uint32_t i = 0; i < size; i++) {
        spi_error_t spi_error = spi_receive_byte(&rx_buffer[i]);
        if (spi_error != SPI_OK) {
            return spi_error;
        }
    }
    
    return SPI_OK;
}

// Одновременная передача и прием буфера
spi_error_t spi_transmit_receive_buffer(
    const uint8_t *tx_buffer,
    uint8_t *rx_buffer,
    uint32_t size)
{
    if (!tx_buffer || !rx_buffer || size == 0) {
        return SPI_ERROR_PARAM;
    }
    
    for (uint32_t i = 0; i < size; i++) {
        spi_error_t result = spi_transmit_receive_byte(tx_buffer[i], &rx_buffer[i]);
        if (result != SPI_OK) {
            return result;
        }
    }
    
    return SPI_OK;
}