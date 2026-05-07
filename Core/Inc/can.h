#ifndef CAN_H
#define CAN_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Конфигурация CAN
// ============================================================================

// Скорость CAN (бит/с)
typedef enum {
    CAN_BAUDRATE_10KBPS    = 10000,
    CAN_BAUDRATE_20KBPS    = 20000,
    CAN_BAUDRATE_50KBPS    = 50000,
    CAN_BAUDRATE_100KBPS   = 100000,
    CAN_BAUDRATE_125KBPS   = 125000,
    CAN_BAUDRATE_250KBPS   = 250000,
    CAN_BAUDRATE_500KBPS   = 500000,
    CAN_BAUDRATE_800KBPS   = 800000,
    CAN_BAUDRATE_1MBPS     = 1000000
} can_baudrate_t;

// Режимы работы CAN
typedef enum {
    CAN_MODE_NORMAL = 0,           // Нормальный режим
    CAN_MODE_LOOPBACK,             // Режим самопроверки
    CAN_MODE_SILENT,               // Только приём (без подтверждения)
    CAN_MODE_SILENT_LOOPBACK       // Самопроверка без подтверждения
} can_mode_t;

// Режим фильтрации
typedef enum {
    CAN_FILTER_MASK_MODE = 0,      // Режим маски
    CAN_FILTER_LIST_MODE           // Режим списка
} can_filter_mode_t;

// Размер фильтра
typedef enum {
    CAN_FILTER_SCALE_16BIT = 0,    // 16-битный фильтр
    CAN_FILTER_SCALE_32BIT         // 32-битный фильтр
} can_filter_scale_t;

// Тип прерываний
typedef enum {
    CAN_IT_TX_MAILBOX_EMPTY = 0,   // Прерывание при освобождении mailbox
    CAN_IT_FIFO0_MESSAGE_PENDING,  // Новое сообщение в FIFO0
    CAN_IT_FIFO0_FULL,             // FIFO0 переполнен
    CAN_IT_FIFO0_OVERFLOW,         // Потеря данных в FIFO0
    CAN_IT_FIFO1_MESSAGE_PENDING,  // Новое сообщение в FIFO1
    CAN_IT_FIFO1_FULL,             // FIFO1 переполнен
    CAN_IT_FIFO1_OVERFLOW,         // Потеря данных в FIFO1
    CAN_IT_ERROR_WARNING,          // Предупреждение об ошибке (>96)
    CAN_IT_ERROR_PASSIVE,          // Пассивный режим ошибок (>127)
    CAN_IT_BUS_OFF,                // Bus Off
    CAN_IT_LAST_ERROR_CODE,        // Код последней ошибки
    CAN_IT_ERROR                   // Любая ошибка CAN
} can_interrupt_t;

// Коды ошибок CAN
typedef enum {
    CAN_ERROR_NONE = 0,            // Нет ошибки
    CAN_ERROR_STUFF,               // Bit stuffing error
    CAN_ERROR_FORM,                // Form error
    CAN_ERROR_ACK,                 // Acknowledgment error
    CAN_ERROR_BIT_RECESSIVE,       // Bit recessive error
    CAN_ERROR_BIT_DOMINANT,        // Bit dominant error
    CAN_ERROR_CRC,                 // CRC error
    CAN_ERROR_SOFTWARE,            // Software error
    CAN_ERROR_BUS_OFF,             // Bus Off
    CAN_ERROR_WARNING,             // Error warning (>96)
    CAN_ERROR_PASSIVE,             // Error passive (>127)
} can_error_code_t;

// Статусы операций
typedef enum {
    CAN_OK = 0,
    CAN_ERROR_BUSY,
    CAN_ERROR_TIMEOUT,
    CAN_ERROR_PARAM,
    CAN_ERROR_NOMESSAGE,
    CAN_ERROR_TX_FAILED,
    CAN_ERROR_FIFO_EMPTY,
    CAN_ERROR_FIFO_FULL,
    CAN_ERROR_MAILBOX_BUSY
} can_error_t;

// ============================================================================
// Структуры данных
// ============================================================================

// CAN сообщение (стандартный идентификатор - 11 бит)
typedef struct {
    uint32_t id;                   // Идентификатор (11 или 29 бит)
    uint8_t data[8];               // Данные (до 8 байт)
    uint8_t dlc;                   // Длина данных (0-8)
    bool is_extended;              // true - расширенный ID (29 бит)
    bool is_remote;                // true - дистанционный кадр (RTR)
} can_message_t;

// Конфигурация CAN
typedef struct {
    can_baudrate_t baudrate;       // Скорость CAN
    can_mode_t mode;               // Режим работы
    uint8_t sync_jump_width;       // Ширина синхронизации (1-4 кванта)
    uint8_t time_segment_1;        // Временной сегмент 1 (1-16 квантов)
    uint8_t time_segment_2;        // Временной сегмент 2 (1-8 квантов)
    bool auto_retransmission;      // Автоматическая ретрансляция
    bool receive_fifo_lock;        // Заморозка FIFO при переполнении
    bool transmit_fifo_priority;   // Приоритет по ID сообщения
} can_config_t;

// Фильтр
typedef struct {
    uint8_t filter_bank;           // Номер банка фильтров (0-27)
    can_filter_mode_t mode;        // Режим (маска/список)
    can_filter_scale_t scale;      // Размер (16/32 бит)
    uint32_t id1;                  // ID1 или ID низкое
    uint32_t id2;                  // ID2 или ID высокое или маска
    bool fifo0_assignment;         // true - FIFO0, false - FIFO1
    bool enable;                   // Включить фильтр
} can_filter_t;

// Статус CAN
typedef struct {
    bool tx_pending[3];            // Ожидание отправки в mailbox
    uint8_t tx_error_counter;      // Счётчик ошибок передачи
    uint8_t rx_error_counter;      // Счётчик ошибок приёма
    can_error_code_t last_error;   // Код последней ошибки
    bool bus_off;                  // Состояние Bus Off
    bool error_warning;            // Предупреждение об ошибке
    bool error_passive;            // Пассивный режим
} can_status_t;

void can_2_init(void);
can_error_t can_2_receive_msg(can_message_t *message);
can_error_t can_2_transmit_msg(can_message_t message);
const char* can_2_error_to_string(can_error_t error);

#endif // CAN_H