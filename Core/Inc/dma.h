#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Enum для номеров потоков DMA2 (Streams 0-7)
// ============================================================================
typedef enum {
    DMA2_STREAM_0 = 0,
    DMA2_STREAM_1 = 1,
    DMA2_STREAM_2 = 2,
    DMA2_STREAM_3 = 3,
    DMA2_STREAM_4 = 4,
    DMA2_STREAM_5 = 5,
    DMA2_STREAM_6 = 6,
    DMA2_STREAM_7 = 7,
    DMA2_STREAM_MAX = 8
} dma2_stream_t;

// ============================================================================
// Enum для каналов DMA2 (Channel 0-7)
// ============================================================================
typedef enum {
    DMA2_CHANNEL_0 = 0,   // ADC1, SPI2_RX, I2C1_RX, USART1_RX и др.
    DMA2_CHANNEL_1 = 1,   // ADC2, SPI3_RX, I2C2_RX, USART2_RX и др.
    DMA2_CHANNEL_2 = 2,   // ADC3, SPI1_RX, I2C3_RX, USART3_RX и др.
    DMA2_CHANNEL_3 = 3,   // SPI1_TX, SPI3_TX, I2C1_TX, USART1_TX и др.
    DMA2_CHANNEL_4 = 4,   // SPI2_TX, I2C2_TX, USART2_TX, TIM1_CH1 и др.
    DMA2_CHANNEL_5 = 5,   // TIM1_CH2, TIM1_CH3, TIM1_CH4 и др.
    DMA2_CHANNEL_6 = 6,   // TIM1_UP, TIM1_TRIG, TIM1_COM и др.
    DMA2_CHANNEL_7 = 7    // TIM1_UP, TIM1_TRIG, TIM1_COM и др.
} dma2_channel_t;

// ============================================================================
// Enum для периферийных устройств (удобные маппинги)
// ============================================================================
typedef enum {
    // SPI2
    DMA2_PERIPH_SPI2_RX = 0,
    DMA2_PERIPH_SPI2_TX = 1,
    
    // USART1
    DMA2_PERIPH_USART1_RX = 2,
    DMA2_PERIPH_USART1_TX = 3,
    
    // USART2
    DMA2_PERIPH_USART2_RX = 4,
    DMA2_PERIPH_USART2_TX = 5,
    
    // I2C1
    DMA2_PERIPH_I2C1_RX = 6,
    DMA2_PERIPH_I2C1_TX = 7,
    
    // I2C2
    DMA2_PERIPH_I2C2_RX = 8,
    DMA2_PERIPH_I2C2_TX = 9,
    
    // ADC1
    DMA2_PERIPH_ADC1 = 10,
    
    // TIM1
    DMA2_PERIPH_TIM1_CH1 = 11,
    DMA2_PERIPH_TIM1_CH2 = 12,
    DMA2_PERIPH_TIM1_CH3 = 13,
    DMA2_PERIPH_TIM1_CH4 = 14,
    DMA2_PERIPH_TIM1_UP = 15,
    DMA2_PERIPH_TIM1_TRIG = 16,
    DMA2_PERIPH_TIM1_COM = 17,
    
    // TIM8
    DMA2_PERIPH_TIM8_CH1 = 18,
    DMA2_PERIPH_TIM8_CH2 = 19,
    DMA2_PERIPH_TIM8_CH3 = 20,
    DMA2_PERIPH_TIM8_CH4 = 21,
    DMA2_PERIPH_TIM8_UP = 22,
    DMA2_PERIPH_TIM8_TRIG = 23,
    DMA2_PERIPH_TIM8_COM = 24,
    
    // SPI1
    DMA2_PERIPH_SPI1_RX = 25,
    DMA2_PERIPH_SPI1_TX = 26,
    
    // SPI3
    DMA2_PERIPH_SPI3_RX = 27,
    DMA2_PERIPH_SPI3_TX = 28
} dma2_periph_t;

// ============================================================================
// Структура для маппинга периферии на поток и канал
// ============================================================================
typedef struct {
    dma2_stream_t stream;   // Поток DMA
    dma2_channel_t channel; // Канал DMA
} dma2_mapping_t;

// ============================================================================
// Предопределенные маппинги для популярной периферии
// ============================================================================
// SPI2
#define DMA2_SPI2_RX_MAPPING   {DMA2_STREAM_3, DMA2_CHANNEL_0}
#define DMA2_SPI2_TX_MAPPING   {DMA2_STREAM_4, DMA2_CHANNEL_0}

// USART1
#define DMA2_USART1_RX_MAPPING {DMA2_STREAM_5, DMA2_CHANNEL_4}
#define DMA2_USART1_TX_MAPPING {DMA2_STREAM_7, DMA2_CHANNEL_4}

// USART2
#define DMA2_USART2_RX_MAPPING {DMA2_STREAM_6, DMA2_CHANNEL_4}
#define DMA2_USART2_TX_MAPPING {DMA2_STREAM_4, DMA2_CHANNEL_4}

// I2C1
#define DMA2_I2C1_RX_MAPPING   {DMA2_STREAM_0, DMA2_CHANNEL_1}
#define DMA2_I2C1_TX_MAPPING   {DMA2_STREAM_6, DMA2_CHANNEL_1}

// ADC1
#define DMA2_ADC1_MAPPING      {DMA2_STREAM_0, DMA2_CHANNEL_0}

// TIM1
#define DMA2_TIM1_CH1_MAPPING  {DMA2_STREAM_4, DMA2_CHANNEL_5}
#define DMA2_TIM1_CH2_MAPPING  {DMA2_STREAM_1, DMA2_CHANNEL_6}
#define DMA2_TIM1_CH3_MAPPING  {DMA2_STREAM_2, DMA2_CHANNEL_6}
#define DMA2_TIM1_CH4_MAPPING  {DMA2_STREAM_3, DMA2_CHANNEL_6}
#define DMA2_TIM1_UP_MAPPING   {DMA2_STREAM_5, DMA2_CHANNEL_6}
#define DMA2_TIM1_TRIG_MAPPING {DMA2_STREAM_6, DMA2_CHANNEL_6}
#define DMA2_TIM1_COM_MAPPING  {DMA2_STREAM_7, DMA2_CHANNEL_6}

// ============================================================================
// Режимы работы DMA
// ============================================================================
typedef enum {
    DMA_MODE_MEM_TO_MEM,      // Память -> Память
    DMA_MODE_PERIPH_TO_MEM,   // Периферия -> Память
    DMA_MODE_MEM_TO_PERIPH    // Память -> Периферия
} dma_mode_t;

// Размер данных
typedef enum {
    DMA_DATA_SIZE_BYTE = 0,   // 8 бит
    DMA_DATA_SIZE_HALF_WORD,  // 16 бит
    DMA_DATA_SIZE_WORD        // 32 бита
} dma_data_size_t;

// Приоритет канала
typedef enum {
    DMA_PRIORITY_LOW = 0,
    DMA_PRIORITY_MEDIUM,
    DMA_PRIORITY_HIGH,
    DMA_PRIORITY_VERY_HIGH
} dma_priority_t;

// Режим циркулярного буфера
typedef enum {
    DMA_CIRCULAR_DISABLED = 0,
    DMA_CIRCULAR_ENABLED
} dma_circular_mode_t;

// Режим инкремента адреса
typedef struct {
    bool peripheral_increment;  // Инкремент адреса периферии
    bool memory_increment;      // Инкремент адреса памяти
} dma_increment_t;

// Конфигурация DMA канала
typedef struct {
    dma_mode_t mode;                    // Режим работы
    dma_data_size_t data_size;          // Размер данных
    dma_priority_t priority;            // Приоритет
    dma_circular_mode_t circular_mode;  // Циркулярный режим
    dma_increment_t increment;          // Настройки инкремента
    bool transfer_complete_interrupt;   // Прерывание по завершении
    bool half_transfer_interrupt;       // Прерывание по полузавершении
    bool transfer_error_interrupt;      // Прерывание по ошибке
    dma2_channel_t channel;             // Канал DMA (для периферии)
} dma_config_t;

// Статус операции
typedef enum {
    DMA_OK = 0,
    DMA_ERROR_BUSY,
    DMA_ERROR_PARAM,
    DMA_ERROR_CHANNEL,
    DMA_ERROR_TIMEOUT,
    DMA_ERROR_TRANSFER
} dma_error_t;

// Callback функция для обработки прерываний
typedef void (*dma_callback_t)(void);

// ============================================================================
// Функции для работы с маппингом
// ============================================================================
// Получить маппинг для периферии
dma2_mapping_t dma_get_mapping(dma2_periph_t periph);

// Получить поток для периферии
dma2_stream_t dma_get_stream(dma2_periph_t periph);

// Получить канал для периферии
dma2_channel_t dma_get_channel(dma2_periph_t periph);

// ============================================================================
// Основные функции DMA
// ============================================================================
// Инициализация DMA контроллера
void dma_init(void);

// Настройка потока DMA
dma_error_t dma_set_config(dma2_stream_t stream, const dma_config_t *config);

// Запуск DMA передачи
dma_error_t dma_start(dma2_stream_t stream,
    uint32_t peripheral_addr,
    uint32_t memory_addr,
    uint32_t data_count
);

// Остановка DMA передачи
dma_error_t dma_stop(dma2_stream_t stream);

// Проверка занятости потока
bool dma_is_busy(dma2_stream_t stream);

// Завершен ли поток
bool dma_is_stream_completed(dma2_stream_t stream);

// Завершен ли поток по ошибке
bool dma_is_stream_failed(dma2_stream_t stream);

// Получение количества оставшихся данных
uint32_t dma_get_remaining(dma2_stream_t stream);

// Очистка флагов прерываний
void dma_clear_flags(dma2_stream_t stream);

// Регистрация callback функций
void dma_set_callback(dma2_stream_t stream, 
    dma_callback_t complete_cb, 
    dma_callback_t half_cb, 
    dma_callback_t error_cb
);

// Блокирующая передача (ожидание завершения)
dma_error_t dma_wait(dma2_stream_t stream, uint32_t timeout_ms);

// ============================================================================
// Специализированные функции для конкретных задач
// ============================================================================
// DMA память-память
dma_error_t dma_memcpy(dma2_stream_t stream, void* dest, const void* src, uint32_t size_bytes);

// ============================================================================
// Удобные функции для конкретной периферии
// ============================================================================
// SPI2
dma_error_t dma_spi2_rx_init(dma2_stream_t stream, void *buffer, uint32_t buffer_size);
dma_error_t dma_spi2_tx_init(dma2_stream_t stream, void *buffer, uint32_t buffer_size);

// USART1
dma_error_t dma_uart1_rx_init(dma2_stream_t stream, void *buffer, uint32_t buffer_size);
dma_error_t dma_uart1_tx_init(dma2_stream_t stream, void *buffer, uint32_t buffer_size);

// USART2
dma_error_t dma_uart2_rx_init(dma2_stream_t stream, void *buffer, uint32_t buffer_size);
dma_error_t dma_uart2_tx_init(dma2_stream_t stream, void *buffer, uint32_t buffer_size);

// ADC1
dma_error_t dma_adc1_init(dma2_stream_t stream, void* buffer_addr, uint32_t buffer_size);

// Функции с автоматическим выбором потока (по маппингу)
dma_error_t dma_adc1_init_auto(void *buffer, uint32_t buffer_size);
dma_error_t dma_spi2_rx_init_auto(void *buffer, uint32_t buffer_size);
dma_error_t dma_spi2_tx_init_auto(void *buffer, uint32_t buffer_size);
dma_error_t dma_uart1_rx_init_auto(void *buffer, uint32_t buffer_size);
dma_error_t dma_uart1_tx_init_auto(void *buffer, uint32_t buffer_size);
dma_error_t dma_uart2_rx_init_auto(void *buffer, uint32_t buffer_size);
dma_error_t dma_uart2_tx_init_auto(void *buffer, uint32_t buffer_size);

#endif // DMA_H