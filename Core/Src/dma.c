#include <stddef.h>
#include <string.h>
#include "stm32f407xx.h"
#include "dma.h"
#include "delay.h"
#include "uart.h"
#include "critical_section.h"

#define DEBUG_MODE            0

// Максимальное количество потоков (Stream) в DMA2
#define DMA2_MAX_STREAMS      8

// Флаги прерываний для каждого потока в LIFCR/HIFCR
#define DMA2_TCIF(stream)          (1 << (6 * (stream)))
#define DMA2_HTIF(stream)          (2 << (6 * (stream)))
#define DMA2_TEIF(stream)          (4 << (6 * (stream)))

// Таймаут по умолчанию (мс)
#define DMA_TIMEOUT_MS          1000

// Статические переменные
static bool is_initialized = false;
static bool stream_initialized[DMA2_MAX_STREAMS] = {false};
static dma_callback_t complete_callback[DMA2_MAX_STREAMS] = {NULL};
static dma_callback_t half_callback[DMA2_MAX_STREAMS] = {NULL};
static dma_callback_t error_callback[DMA2_MAX_STREAMS] = {NULL};
static bool is_transfer_completed[DMA2_MAX_STREAMS] = {false};
static bool is_transfer_failed[DMA2_MAX_STREAMS] = {false};

// Таблица маппинга периферии на потоки и каналы
static const dma2_mapping_t dma_mapping_table[] = {
    [DMA2_PERIPH_SPI2_RX]    = DMA2_SPI2_RX_MAPPING,
    [DMA2_PERIPH_SPI2_TX]    = DMA2_SPI2_TX_MAPPING,
    [DMA2_PERIPH_USART1_RX]  = DMA2_USART1_RX_MAPPING,
    [DMA2_PERIPH_USART1_TX]  = DMA2_USART1_TX_MAPPING,
    [DMA2_PERIPH_USART2_RX]  = DMA2_USART2_RX_MAPPING,
    [DMA2_PERIPH_USART2_TX]  = DMA2_USART2_TX_MAPPING,
    [DMA2_PERIPH_ADC1]       = DMA2_ADC1_MAPPING,
    [DMA2_PERIPH_TIM1_CH1]   = DMA2_TIM1_CH1_MAPPING,
    [DMA2_PERIPH_TIM1_CH2]   = DMA2_TIM1_CH2_MAPPING,
    [DMA2_PERIPH_TIM1_CH3]   = DMA2_TIM1_CH3_MAPPING,
    [DMA2_PERIPH_TIM1_CH4]   = DMA2_TIM1_CH4_MAPPING,
    [DMA2_PERIPH_TIM1_UP]    = DMA2_TIM1_UP_MAPPING,
    [DMA2_PERIPH_TIM1_TRIG]  = DMA2_TIM1_TRIG_MAPPING,
    [DMA2_PERIPH_TIM1_COM]   = DMA2_TIM1_COM_MAPPING,
};

// Получение указателя на Stream по номеру
static DMA_Stream_TypeDef* get_stream(dma2_stream_t stream)
{
    if (stream >= DMA2_MAX_STREAMS) {
        return NULL;
    }
    return DMA2_Stream0 + stream;
}

// ============================================================================
// Функции для работы с маппингом
// ============================================================================

dma2_mapping_t dma_get_mapping(dma2_periph_t periph)
{
    if (periph >= 0 && periph < sizeof(dma_mapping_table) / sizeof(dma_mapping_table[0])) {
        return dma_mapping_table[periph];
    }
    
    dma2_mapping_t invalid = {DMA2_STREAM_MAX, (dma2_channel_t)8};
    return invalid;
}

dma2_stream_t dma_get_stream(dma2_periph_t periph)
{
    return dma_get_mapping(periph).stream;
}

dma2_channel_t dma_get_channel(dma2_periph_t periph)
{
    return dma_get_mapping(periph).channel;
}

// ============================================================================
// Инициализация DMA
// ============================================================================

void dma_init(void)
{
    if (is_initialized) {
        return;
    }

    // Включить тактирование DMA2
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    
    // Сбросить все потоки
    for (uint8_t i = 0; i < DMA2_MAX_STREAMS; i++) {
        DMA_Stream_TypeDef *stream = get_stream((dma2_stream_t)i);
        
        stream->CR &= ~DMA_SxCR_EN;
        while (stream->CR & DMA_SxCR_EN) {
            __NOP();
        }
        
        stream->CR = 0;
        stream->NDTR = 0;
        stream->PAR = 0;
        stream->M0AR = 0;
        stream->M1AR = 0;
        stream->FCR = 0;
        
        stream_initialized[i] = false;
        complete_callback[i] = NULL;
        half_callback[i] = NULL;
        error_callback[i] = NULL;

        is_transfer_completed[i] = false;
        is_transfer_failed[i] = false;
    }
    
    // Сбрасываем все флаги прерываний DMA2
    DMA2->LIFCR = 0xFFFFFFFF;
    DMA2->HIFCR = 0xFFFFFFFF;
    
    // Настройка NVIC для DMA2
    NVIC_SetPriority(DMA2_Stream0_IRQn, 0x0C);
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    
    NVIC_SetPriority(DMA2_Stream1_IRQn, 0x0C);
    NVIC_EnableIRQ(DMA2_Stream1_IRQn);
    
    NVIC_SetPriority(DMA2_Stream2_IRQn, 0x0C);
    NVIC_EnableIRQ(DMA2_Stream2_IRQn);
    
    NVIC_SetPriority(DMA2_Stream3_IRQn, 0x0C);
    NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    
    NVIC_SetPriority(DMA2_Stream4_IRQn, 0x0C);
    NVIC_EnableIRQ(DMA2_Stream4_IRQn);
    
    NVIC_SetPriority(DMA2_Stream5_IRQn, 0x0C);
    NVIC_EnableIRQ(DMA2_Stream5_IRQn);
    
    NVIC_SetPriority(DMA2_Stream6_IRQn, 0x0C);
    NVIC_EnableIRQ(DMA2_Stream6_IRQn);
    
    NVIC_SetPriority(DMA2_Stream7_IRQn, 0x0C);
    NVIC_EnableIRQ(DMA2_Stream7_IRQn);
    
    is_initialized = true;
    uart_send_line("DMA2 initialized");
}

// ============================================================================
// Основные функции DMA
// ============================================================================

dma_error_t dma_set_config(dma2_stream_t stream, const dma_config_t *config)
{
    if (!config || stream >= DMA2_MAX_STREAMS) {
        return DMA_ERROR_PARAM;
    }
    
    DMA_Stream_TypeDef *dma_stream = get_stream(stream);
    if (!dma_stream) {
        return DMA_ERROR_CHANNEL;
    }
    
    if (dma_stream->CR & DMA_SxCR_EN) {
        return DMA_ERROR_BUSY;
    }
    
    uint32_t basepri = critical_enter_basp();
    uint32_t cr = 0;
    dma_stream->CR = 0;
    
    // Настройка направления передачи
    switch (config->mode) {
        case DMA_MODE_PERIPH_TO_MEM:
            cr &= ~DMA_SxCR_DIR;
            break;
        case DMA_MODE_MEM_TO_PERIPH:
            cr &= ~DMA_SxCR_DIR;
            cr |= DMA_SxCR_DIR_0;
            break;
        case DMA_MODE_MEM_TO_MEM:
            cr &= ~DMA_SxCR_DIR;
            cr |= DMA_SxCR_DIR_1;
            break;
        default:
            critical_exit_basp(basepri);
            return DMA_ERROR_PARAM;
    }
    
    // Настройка размера данных
    switch (config->data_size) {
        case DMA_DATA_SIZE_BYTE:
            cr &= ~(DMA_SxCR_MSIZE | DMA_SxCR_PSIZE);
            break;
        case DMA_DATA_SIZE_HALF_WORD:
            cr |= DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0;
            break;
        case DMA_DATA_SIZE_WORD:
            cr |= DMA_SxCR_PSIZE_1 | DMA_SxCR_MSIZE_1;
            break;
        default:
            break;
    }
    
    // Настройка приоритета
    cr |= ((uint32_t)config->priority << DMA_SxCR_PL_Pos);
    
    // Циркулярный режим
    if (config->circular_mode == DMA_CIRCULAR_ENABLED && config->mode != DMA_MODE_MEM_TO_MEM) {
        cr |= DMA_SxCR_CIRC;
    }
    
    // Инкремент адресов
    if (config->increment.peripheral_increment) {
        cr |= DMA_SxCR_PINC;
    }
    if (config->increment.memory_increment) {
        cr |= DMA_SxCR_MINC;
    }
    
    // Настройка прерываний
    if (config->transfer_complete_interrupt) {
        cr |= DMA_SxCR_TCIE;
    }
    if (config->half_transfer_interrupt) {
        cr |= DMA_SxCR_HTIE;
    }
    if (config->transfer_error_interrupt) {
        cr |= DMA_SxCR_TEIE;
    }
    
    // Настройка канала (для периферийных операций)
    if (config->mode != DMA_MODE_MEM_TO_MEM) {
        cr |= ((uint32_t) config->channel << DMA_SxCR_CHSEL_Pos);
    }

    // Настройка FIFO для UART
    switch (config->mode) {
        case DMA_MODE_MEM_TO_PERIPH:
        case DMA_MODE_PERIPH_TO_MEM:
            dma_stream->FCR |= DMA_SxFCR_DMDIS;
            break;
        case DMA_MODE_MEM_TO_MEM:
            dma_stream->FCR &= ~DMA_SxFCR_DMDIS;
            break;
        default:            
            break;
    }
            
    dma_stream->CR = cr;

    critical_exit_basp(basepri);

    stream_initialized[stream] = true;

    if (DEBUG_MODE) {
        uart_printf_line("DMA stream %d configured: mode=%d, data_size=%d, priority=%d",
            stream, 
            config->mode, 
            config->data_size, 
            config->priority
        );
    }
    
    return DMA_OK;
}

dma_error_t dma_start(dma2_stream_t stream,
    uint32_t peripheral_addr,
    uint32_t memory_addr,
    uint32_t data_count)
{
    if (stream >= DMA2_MAX_STREAMS || !stream_initialized[stream]) {
        return DMA_ERROR_PARAM;
    }
    
    if (data_count == 0 || data_count > 0xFFFF) {
        return DMA_ERROR_PARAM;
    }

    DMA_Stream_TypeDef *dma_stream = get_stream(stream);
    if (!dma_stream) {
        return DMA_ERROR_CHANNEL;
    }

    if (dma_stream->CR & DMA_SxCR_EN) {
        return DMA_ERROR_BUSY;
    }

    uint32_t basepri = critical_enter_basp();

    dma_stream->PAR = peripheral_addr;
    dma_stream->M0AR = memory_addr;
    dma_stream->NDTR = data_count;

    // Сбрасываем флаги прерываний
    dma_clear_flags(stream);

    dma_stream->CR |= DMA_SxCR_EN;

    critical_exit_basp(basepri);

    if (DEBUG_MODE) {
        uart_printf_line("DMA stream %d started: count=%lu, periph_addr=0x%08lX, mem_addr=0x%08lX",
            stream, 
            data_count, 
            peripheral_addr,
            memory_addr
        );
    }

    return DMA_OK;
}

dma_error_t dma_stop(dma2_stream_t stream)
{
    if (stream >= DMA2_MAX_STREAMS) {
        return DMA_ERROR_PARAM;
    }
    
    DMA_Stream_TypeDef *dma_stream = get_stream(stream);
    if (!dma_stream) {
        return DMA_ERROR_CHANNEL;
    }

    uint32_t basepri = critical_enter_basp();

    dma_stream->CR &= ~DMA_SxCR_EN;

    bool timeout = false;
    uint32_t start_tick = get_tick_ms();
    while (dma_stream->CR & DMA_SxCR_EN) {
        if ((get_tick_ms() - start_tick) > DMA_TIMEOUT_MS) {
            timeout = true;
            break;
        }
    }

    critical_exit_basp(basepri);
    
    if (timeout) {
        return DMA_ERROR_TIMEOUT;
    }
    
    if (DEBUG_MODE) {
        uart_printf_line("DMA stream %d stopped", stream);
    }
    
    return DMA_OK;
}

bool dma_is_busy(dma2_stream_t stream)
{
    if (stream >= DMA2_MAX_STREAMS) {
        return false;
    }
    
    DMA_Stream_TypeDef *dma_stream = get_stream(stream);
    if (!dma_stream) {
        return false;
    }

    // bool r1 = dma_is_stream_completed(stream);
    // bool r2 = dma_is_stream_failed(stream);
    // bool r3 = !r1 && !r2; 

    // return r3;
    return (dma_stream->CR & DMA_SxCR_EN);
}

bool dma_is_stream_completed(dma2_stream_t stream)
{
    if (stream >= DMA2_MAX_STREAMS) {
        return false;
    }
    
    DMA_Stream_TypeDef *dma_stream = get_stream(stream);
    if (!dma_stream) {
        return false;
    }

    bool result;

    CRITICAL_SECTION_START();
    result = is_transfer_completed[stream];
    CRITICAL_SECTION_END();

    return result;
}

bool dma_is_stream_failed(dma2_stream_t stream)
{
    if (stream >= DMA2_MAX_STREAMS) {
        return false;
    }
    
    DMA_Stream_TypeDef *dma_stream = get_stream(stream);
    if (!dma_stream) {
        return false;
    }

    bool result;

    CRITICAL_SECTION_START();
    result = is_transfer_failed[stream];
    CRITICAL_SECTION_END();

    return result;
}

uint32_t dma_get_remaining(dma2_stream_t stream)
{
    if (stream >= DMA2_MAX_STREAMS) {
        return 0;
    }
    
    DMA_Stream_TypeDef *dma_stream = get_stream(stream);
    if (!dma_stream) {
        return 0;
    }
    
    return dma_stream->NDTR;
}

void dma_clear_flags(dma2_stream_t stream)
{
    if (stream >= DMA2_MAX_STREAMS) {
        return;
    }

    is_transfer_failed[stream] = false;
    is_transfer_completed[stream] = false;
    
    if (stream < 4) {
        DMA2->LIFCR = DMA2_TCIF(stream) | DMA2_HTIF(stream) | DMA2_TEIF(stream);
    } else {
        DMA2->HIFCR = DMA2_TCIF(stream - 4) | DMA2_HTIF(stream - 4) | DMA2_TEIF(stream - 4);
    }
}

void dma_set_callback(dma2_stream_t stream, 
    dma_callback_t complete_cb, 
    dma_callback_t half_cb, 
    dma_callback_t error_cb)
{
    if (stream >= DMA2_MAX_STREAMS) {
        return;
    }
    
    uint32_t basepri = critical_enter_basp();
    
    complete_callback[stream] = complete_cb;
    half_callback[stream] = half_cb;
    error_callback[stream] = error_cb;
    
    critical_exit_basp(basepri);
}

dma_error_t dma_wait(dma2_stream_t stream, uint32_t timeout_ms)
{
    if (stream >= DMA2_MAX_STREAMS) {
        return DMA_ERROR_PARAM;
    }
    
    uint32_t start_tick = get_tick_ms();
    
    while ( dma_is_busy(stream) ) {
        if ((get_tick_ms() - start_tick) > timeout_ms) {
            dma_stop(stream);
            return DMA_ERROR_TIMEOUT;
        }
    }
    
    return DMA_OK;
}

// ============================================================================
// Специализированные функции
// ============================================================================

dma_error_t dma_memcpy(dma2_stream_t stream, void* dest, const void* src, uint32_t size_bytes)
{
    if (stream != DMA2_STREAM_0) {
        return DMA_ERROR_PARAM;  // Mem-to-Mem только на Stream 0
    }
    
    if (dest == NULL || src == NULL || size_bytes == 0) {
        return DMA_ERROR_PARAM;
    }
    
    dma_config_t config = {
        .mode = DMA_MODE_MEM_TO_MEM,
        .data_size = DMA_DATA_SIZE_BYTE,
        .priority = DMA_PRIORITY_HIGH,
        .circular_mode = DMA_CIRCULAR_DISABLED,
        .increment = {
            .peripheral_increment = true,
            .memory_increment = true
        },
        .transfer_complete_interrupt = true,
        .half_transfer_interrupt = false,
        .transfer_error_interrupt = true,
        .channel = DMA2_CHANNEL_0
    };

    dma_error_t dma_error = dma_stop(stream);
    if (dma_error != DMA_OK) {
        return dma_error;
    }
    
    dma_error = dma_set_config(stream, &config);
    if (dma_error != DMA_OK) {
        return dma_error;
    }

    dma_error = dma_start(stream, 
        (uint32_t)src, 
        (uint32_t)dest, 
        size_bytes
    );
    
    return dma_error;
}

// SPI2
dma_error_t dma_spi2_rx_init(dma2_stream_t stream, void *buffer_addr, uint32_t buffer_size)
{
    if (stream != DMA2_STREAM_3) {
        return DMA_ERROR_PARAM;  // Mem-to-Mem только на Stream 0
    }

    if (NULL == buffer_addr || 0 == buffer_size) {
        return DMA_ERROR_PARAM;
    }

    dma_config_t config = {
        .mode = DMA_MODE_PERIPH_TO_MEM,
        .data_size = DMA_DATA_SIZE_BYTE,
        .priority = DMA_PRIORITY_MEDIUM,
        .circular_mode = DMA_CIRCULAR_DISABLED,
        .increment = {
            .peripheral_increment = false,
            .memory_increment = true
        },
        .transfer_complete_interrupt = true,
        .half_transfer_interrupt = false,
        .transfer_error_interrupt = true,
        .channel = DMA2_CHANNEL_0
    };

    dma_error_t dma_error = dma_stop(stream);
    if (dma_error != DMA_OK) {
        return dma_error;
    }
    
    dma_error = dma_set_config(stream, &config);
    if (dma_error != DMA_OK) {
        return dma_error;
    }

    dma_error = dma_start(stream,
        (uint32_t)&SPI2->DR, 
        (uint32_t)buffer_addr, 
        buffer_size
    );

    return dma_error;
}

dma_error_t dma_spi2_tx_init(dma2_stream_t stream, void *buffer_addr, uint32_t buffer_size)
{
    if (stream != DMA2_STREAM_4) {
        return DMA_ERROR_PARAM;  // Mem-to-Mem только на Stream 0
    }

    if (NULL == buffer_addr || 0 == buffer_size) {
        return DMA_ERROR_PARAM;
    }

    dma_config_t config = {
        .mode = DMA_MODE_MEM_TO_PERIPH,
        .data_size = DMA_DATA_SIZE_BYTE,
        .priority = DMA_PRIORITY_MEDIUM,
        .circular_mode = DMA_CIRCULAR_DISABLED,
        .increment = {
            .peripheral_increment = false,
            .memory_increment = true
        },
        .transfer_complete_interrupt = true,
        .half_transfer_interrupt = false,
        .transfer_error_interrupt = true,
        .channel = DMA2_CHANNEL_0
    };

    dma_error_t dma_error = dma_stop(stream);
    if (dma_error != DMA_OK) {
        return dma_error;
    }
    
    dma_error = dma_set_config(stream, &config);
    if (dma_error != DMA_OK) {
        return dma_error;
    }
    
    dma_error = dma_start(stream, 
        (uint32_t)&SPI2->DR, 
        (uint32_t)buffer_addr, 
        buffer_size
    );

    return dma_error;
}

// USART1
dma_error_t dma_uart1_rx_init(dma2_stream_t stream, void *buffer_addr, uint32_t buffer_size)
{
    if (stream != DMA2_STREAM_5) {
        return DMA_ERROR_PARAM;  // Mem-to-Mem только на Stream 0
    }

    if (NULL == buffer_addr || 0 == buffer_size) {
        return DMA_ERROR_PARAM;
    }

    dma_config_t config = {
        .mode = DMA_MODE_PERIPH_TO_MEM,
        .data_size = DMA_DATA_SIZE_BYTE,
        .priority = DMA_PRIORITY_MEDIUM,
        .circular_mode = DMA_CIRCULAR_ENABLED,
        .increment = {
            .peripheral_increment = false,
            .memory_increment = true
        },
        .transfer_complete_interrupt = true,
        .half_transfer_interrupt = true,
        .transfer_error_interrupt = true,
        .channel = DMA2_CHANNEL_4
    };

    dma_error_t dma_error = dma_stop(stream);
    if (dma_error != DMA_OK) {
        return dma_error;
    }
    
    dma_error = dma_set_config(stream, &config);
    if (dma_error != DMA_OK) {
        return dma_error;
    }

    dma_error = dma_start(stream, 
        (uint32_t)&USART1->DR, 
        (uint32_t) buffer_addr, 
        buffer_size
    );
    
    return dma_error;
}

dma_error_t dma_uart1_tx_init(dma2_stream_t stream, void *buffer_addr, uint32_t buffer_size)
{    
    // Убеждаемся, что USART1 включён и DMAT установлен
    if (!(USART1->CR3 & USART_CR3_DMAT)) {
        return DMA_ERROR_TRANSFER;  // DMA для UART не включён
    }

    if (stream != DMA2_STREAM_7) {
        return DMA_ERROR_PARAM;
    }

    if (NULL == buffer_addr || 0 == buffer_size) {
        return DMA_ERROR_PARAM;
    }

    dma_config_t config = {
        .mode = DMA_MODE_MEM_TO_PERIPH,
        .data_size = DMA_DATA_SIZE_BYTE,
        .priority = DMA_PRIORITY_HIGH,
        .circular_mode = DMA_CIRCULAR_DISABLED,
        .increment = {
            .peripheral_increment = false,
            .memory_increment = true
        },
        .transfer_complete_interrupt = true,
        .half_transfer_interrupt = false,
        .transfer_error_interrupt = true,
        .channel = DMA2_CHANNEL_4
    };

    dma_error_t dma_error = dma_stop(stream);
    if (dma_error != DMA_OK) {
        return dma_error;
    }
    
    dma_error = dma_set_config(stream, &config);
    if (dma_error != DMA_OK) {
        return dma_error;
    }

    dma_error = dma_start(stream, 
        (uint32_t)&USART1->DR,
        (uint32_t) buffer_addr,
        buffer_size
    );
    
    return dma_error;
}

// USART2
dma_error_t dma_uart2_rx_init(dma2_stream_t stream, void *buffer_addr, uint32_t buffer_size)
{
    if (stream != DMA2_STREAM_6) {
        return DMA_ERROR_PARAM;  // Mem-to-Mem только на Stream 0
    }

    if (NULL == buffer_addr || 0 == buffer_size) {
        return DMA_ERROR_PARAM;
    }

    dma_config_t config = {
        .mode = DMA_MODE_PERIPH_TO_MEM,
        .data_size = DMA_DATA_SIZE_BYTE,
        .priority = DMA_PRIORITY_MEDIUM,
        .circular_mode = DMA_CIRCULAR_ENABLED,
        .increment = {
            .peripheral_increment = false,
            .memory_increment = true
        },
        .transfer_complete_interrupt = true,
        .half_transfer_interrupt = true,
        .transfer_error_interrupt = true,
        .channel = DMA2_CHANNEL_4
    };

    dma_error_t dma_error = dma_stop(stream);
    if (dma_error != DMA_OK) {
        return dma_error;
    }
    
    dma_error = dma_set_config(stream, &config);
    if (dma_error != DMA_OK) {
        return dma_error;
    }

    dma_error = dma_start(stream, 
        (uint32_t)&USART1->DR, 
        (uint32_t) buffer_addr, 
        buffer_size
    );
    
    return dma_error;
}

dma_error_t dma_uart2_tx_init(dma2_stream_t stream, void *buffer_addr, uint32_t buffer_size)
{
    if (stream != DMA2_STREAM_4) {
        return DMA_ERROR_PARAM;  // Mem-to-Mem только на Stream 0
    }

    if (NULL == buffer_addr || 0 == buffer_size) {
        return DMA_ERROR_PARAM;
    }

    dma_config_t config = {
        .mode = DMA_MODE_MEM_TO_PERIPH,
        .data_size = DMA_DATA_SIZE_BYTE,
        .priority = DMA_PRIORITY_MEDIUM,
        .circular_mode = DMA_CIRCULAR_DISABLED,
        .increment = {
            .peripheral_increment = false,
            .memory_increment = true
        },
        .transfer_complete_interrupt = false,
        .half_transfer_interrupt = false,
        .transfer_error_interrupt = false,
        .channel = DMA2_CHANNEL_4
    };

    dma_error_t dma_error = dma_stop(stream);
    if (dma_error != DMA_OK) {
        return dma_error;
    }
    
    dma_error = dma_set_config(stream, &config);
    if (dma_error != DMA_OK) {
        return dma_error;
    }

    dma_error = dma_start(stream, 
        (uint32_t)&USART1->DR,
        (uint32_t)buffer_addr,
        buffer_size
    );
    
    return dma_error;
}

// ADC1
dma_error_t dma_adc1_init(dma2_stream_t stream, void* buffer_addr, uint32_t buffer_size)
{
    if (stream != DMA2_STREAM_0) {
        return DMA_ERROR_PARAM;  // Mem-to-Mem только на Stream 0
    }

    if (NULL == buffer_addr || 0 == buffer_size) {
        return DMA_ERROR_PARAM;
    }

    dma_config_t config = {
        .mode = DMA_MODE_PERIPH_TO_MEM,
        .data_size = DMA_DATA_SIZE_HALF_WORD,
        .priority = DMA_PRIORITY_HIGH,
        .circular_mode = DMA_CIRCULAR_ENABLED,
        .increment = {
            .peripheral_increment = false,
            .memory_increment = true
        },
        .transfer_complete_interrupt = true,
        .half_transfer_interrupt = true,
        .transfer_error_interrupt = true,
        .channel = DMA2_CHANNEL_0
    };

    dma_error_t dma_error = dma_stop(stream);
    if (dma_error != DMA_OK) {
        return dma_error;
    }
    
    dma_error = dma_set_config(stream, &config);
    if (dma_error != DMA_OK) {
        return dma_error;
    }
    
    dma_error = dma_start(stream, 
        (uint32_t)&ADC1->DR, 
        (uint32_t)buffer_addr, 
        buffer_size
    );

    return dma_error;
}

// ============================================================================
// Функции с автоматическим выбором потока
// ============================================================================

dma_error_t dma_spi2_rx_init_auto(void* buffer_addr, uint32_t buffer_size)
{
    return dma_spi2_rx_init(dma_get_stream(DMA2_PERIPH_SPI2_RX), buffer_addr, buffer_size);
}

dma_error_t dma_spi2_tx_init_auto(void* buffer_addr, uint32_t buffer_size)
{
    return dma_spi2_tx_init(dma_get_stream(DMA2_PERIPH_SPI2_TX), buffer_addr, buffer_size);
}

dma_error_t dma_uart1_rx_init_auto(void* buffer_addr, uint32_t buffer_size)
{
    return dma_uart1_rx_init(dma_get_stream(DMA2_PERIPH_USART1_RX), buffer_addr, buffer_size);
}

dma_error_t dma_uart1_tx_init_auto(void* buffer_addr, uint32_t buffer_size)
{
    return dma_uart1_tx_init(dma_get_stream(DMA2_PERIPH_USART1_TX), buffer_addr, buffer_size);
}

dma_error_t dma_uart2_rx_init_auto(void* buffer_addr, uint32_t buffer_size)
{
    return dma_uart2_rx_init(dma_get_stream(DMA2_PERIPH_USART2_RX), buffer_addr, buffer_size);
}

dma_error_t dma_uart2_tx_init_auto(void* buffer_addr, uint32_t buffer_size)
{
    return dma_uart2_tx_init(dma_get_stream(DMA2_PERIPH_USART2_TX), buffer_addr, buffer_size);
}

dma_error_t dma_adc1_init_auto(void* buffer_addr, uint32_t buffer_size)
{
    return dma_adc1_init(dma_get_stream(DMA2_PERIPH_ADC1), buffer_addr, buffer_size);
}

// ============================================================================
// Обработчики прерываний DMA2
// ============================================================================

void DMA2_Stream0_IRQHandler(void)
{
    uint32_t dma_stream_id = 0;
    uint32_t flags = DMA2->LISR;
    
    if (flags & DMA_LISR_TCIF0) {
        DMA2->LIFCR = DMA_LIFCR_CTCIF0;

        uint32_t basepri = critical_enter_basp();
        is_transfer_completed[dma_stream_id] = true;
        critical_exit_basp(basepri);

        if (complete_callback[dma_stream_id]) {
            complete_callback[dma_stream_id]();
        }
    }
    
    if (flags & DMA_LISR_HTIF0) {
        DMA2->LIFCR = DMA_LIFCR_CHTIF0;

        if (half_callback[dma_stream_id]) {
            half_callback[dma_stream_id]();
        }
    }
    
    if (flags & DMA_LISR_TEIF0) {
        DMA2->LIFCR = DMA_LIFCR_CTEIF0;

        uint32_t basepri = critical_enter_basp();
        is_transfer_failed[dma_stream_id] = true;
        critical_exit_basp(basepri);

        if (error_callback[dma_stream_id]) {
            error_callback[dma_stream_id]();
        }
    }
}

void DMA2_Stream1_IRQHandler(void)
{
    uint32_t dma_stream_id = 1;
    uint32_t flags = DMA2->LISR;
    
    if (flags & DMA_LISR_TCIF1) {
        DMA2->LIFCR = DMA_LIFCR_CTCIF1;
        
        uint32_t basepri = critical_enter_basp();
        is_transfer_completed[dma_stream_id] = true;
        critical_exit_basp(basepri);

        if (complete_callback[dma_stream_id]) {
            complete_callback[dma_stream_id]();
        }
    }
    
    if (flags & DMA_LISR_HTIF1) {
        DMA2->LIFCR = DMA_LIFCR_CHTIF1;

        if (half_callback[dma_stream_id]) {
            half_callback[dma_stream_id]();
        }
    }
    
    if (flags & DMA_LISR_TEIF1) {
        DMA2->LIFCR = DMA_LIFCR_CTEIF1;

        uint32_t basepri = critical_enter_basp();
        is_transfer_failed[dma_stream_id] = true;
        critical_exit_basp(basepri);

        if (error_callback[dma_stream_id]) {
            error_callback[dma_stream_id]();
        }
    }
}

void DMA2_Stream2_IRQHandler(void)
{
    uint32_t dma_stream_id = 2;
    uint32_t flags = DMA2->LISR;
    
    if (flags & DMA_LISR_TCIF2) {
        DMA2->LIFCR = DMA_LIFCR_CTCIF2;
        
        uint32_t basepri = critical_enter_basp();
        is_transfer_completed[dma_stream_id] = true;
        critical_exit_basp(basepri);

        if (complete_callback[dma_stream_id]) {
            complete_callback[dma_stream_id]();
        }
    }
    
    if (flags & DMA_LISR_HTIF2) {
        DMA2->LIFCR = DMA_LIFCR_CHTIF2;

        if (half_callback[dma_stream_id]) {
            half_callback[dma_stream_id]();
        }
    }
    
    if (flags & DMA_LISR_TEIF2) {
        DMA2->LIFCR = DMA_LIFCR_CTEIF2;
        
        uint32_t basepri = critical_enter_basp();
        is_transfer_failed[dma_stream_id] = true;
        critical_exit_basp(basepri);

        if (error_callback[dma_stream_id]) {
            error_callback[dma_stream_id]();
        }
    }
}

void DMA2_Stream3_IRQHandler(void)
{
    uint32_t dma_stream_id = 3;
    uint32_t flags = DMA2->LISR; 

    if (flags & DMA_LISR_TCIF3) {
        DMA2->LIFCR = DMA_LIFCR_CTCIF3;

        uint32_t basepri = critical_enter_basp();
        is_transfer_completed[dma_stream_id] = true;
        critical_exit_basp(basepri);

        if (complete_callback[dma_stream_id]) {
            complete_callback[dma_stream_id]();
        }
    }
    
    if (flags & DMA_LISR_HTIF3) {
        DMA2->LIFCR = DMA_LIFCR_CHTIF3;

        if (half_callback[dma_stream_id]) {
            half_callback[dma_stream_id]();
        }
    }
    
    if (flags & DMA_LISR_TEIF3) {
        DMA2->LIFCR = DMA_LIFCR_CTEIF3;
        
        uint32_t basepri = critical_enter_basp();
        is_transfer_failed[dma_stream_id] = true;
        critical_exit_basp(basepri);

        if (error_callback[dma_stream_id]) {
            error_callback[dma_stream_id]();
        }
    }
}

void DMA2_Stream4_IRQHandler(void)
{
    uint32_t dma_stream_id = 4;
    uint32_t flags = DMA2->HISR;
    
    if (flags & DMA_HISR_TCIF4) {
        DMA2->HIFCR = DMA_HIFCR_CTCIF4;
        
        uint32_t basepri = critical_enter_basp();
        is_transfer_completed[4] = true;
        critical_exit_basp(basepri);

        if (complete_callback[dma_stream_id]) {
            complete_callback[dma_stream_id]();
        }
    }
    
    if (flags & DMA_HIFCR_CHTIF4) {
        DMA2->HIFCR = DMA_HIFCR_CHTIF4;

        if (half_callback[dma_stream_id]) {
            half_callback[dma_stream_id]();
        }
    }
    
    if (flags & DMA_HIFCR_CTEIF4) {
        DMA2->HIFCR = DMA_HIFCR_CTEIF4;
        
        uint32_t basepri = critical_enter_basp();
        is_transfer_failed[dma_stream_id] = true;
        critical_exit_basp(basepri);

        if (error_callback[dma_stream_id]) {
            error_callback[dma_stream_id]();
        }
    }
}

void DMA2_Stream5_IRQHandler(void)
{
    uint32_t dma_stream_id = 5;
    uint32_t flags = DMA2->HISR;
    
    if (flags & DMA_HISR_TCIF5) {
        DMA2->HIFCR = DMA_HIFCR_CTCIF5;
        
        uint32_t basepri = critical_enter_basp();
        is_transfer_completed[dma_stream_id] = true;
        critical_exit_basp(basepri);

        if (complete_callback[dma_stream_id]) {
            complete_callback[dma_stream_id]();
        }
    }
    
    if (flags & DMA_HIFCR_CHTIF5) {
        DMA2->HIFCR = DMA_HIFCR_CHTIF5;

        if (half_callback[dma_stream_id]) {
            half_callback[dma_stream_id]();
        }
    }
    
    if (flags & DMA_HIFCR_CTEIF5) {
        DMA2->HIFCR = DMA_HIFCR_CTEIF5;
        
        uint32_t basepri = critical_enter_basp();
        is_transfer_failed[dma_stream_id] = true;
        critical_exit_basp(basepri);

        if (error_callback[dma_stream_id]) {
            error_callback[dma_stream_id]();
        }
    }
}

void DMA2_Stream6_IRQHandler(void)
{
    uint32_t dma_stream_id = 6;
    uint32_t flags = DMA2->HISR;
    
    if (flags & DMA_HISR_TCIF6) {
        DMA2->HIFCR = DMA_HIFCR_CTCIF6;

        uint32_t basepri = critical_enter_basp();
        is_transfer_completed[6] = true;
        critical_exit_basp(basepri);

        if (complete_callback[dma_stream_id]) {
            complete_callback[dma_stream_id]();
        }
    }

    if (flags & DMA_HIFCR_CHTIF6) {
        DMA2->HIFCR = DMA_HIFCR_CHTIF6;

        if (half_callback[dma_stream_id]) {
            half_callback[dma_stream_id]();
        }
    }

    if (flags & DMA_HIFCR_CTEIF6) {
        DMA2->HIFCR = DMA_HIFCR_CTEIF6;
        
        uint32_t basepri = critical_enter_basp();
        is_transfer_failed[dma_stream_id] = true;
        critical_exit_basp(basepri);

        if (error_callback[dma_stream_id]) {
            error_callback[dma_stream_id]();
        }
    }
}

void DMA2_Stream7_IRQHandler(void)
{
    uint32_t dma_stream_id = 7;
    uint32_t flags = DMA2->HISR;
    
    if (flags & DMA_HISR_TCIF7) {
        DMA2->HIFCR = DMA_HIFCR_CTCIF7;

        uint32_t basepri = critical_enter_basp();
        is_transfer_completed[dma_stream_id] = true;
        critical_exit_basp(basepri);

        if (complete_callback[dma_stream_id]) {
            complete_callback[dma_stream_id]();
        }
    }
    
    if (flags & DMA_HIFCR_CHTIF7) {
        DMA2->HIFCR = DMA_HIFCR_CHTIF7;

        if (half_callback[dma_stream_id]) {
            half_callback[dma_stream_id]();
        }
    }
    
    if (flags & DMA_HIFCR_CTEIF7) {
        DMA2->HIFCR = DMA_HIFCR_CTEIF7;
        
        uint32_t basepri = critical_enter_basp();
        is_transfer_failed[dma_stream_id] = true;
        critical_exit_basp(basepri);

        if (error_callback[dma_stream_id]) {
            error_callback[dma_stream_id]();
        }
    }
}