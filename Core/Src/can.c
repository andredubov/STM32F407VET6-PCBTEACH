#include <string.h>
#include "stm32f407xx.h"
#include "can.h"
#include "uart.h"
#include "delay.h"

#define FRAME_1_ID     0x234
#define FRAME_2_ID     0x432
#define CAN_TIMEOUT_MS  1000

void can_2_init(void)
{
    uint32_t timeout;

    // 1. Включение тактирования
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
    RCC->APB1ENR |= RCC_APB1ENR_CAN2EN;

    // 2. Настройка пинов CAN2 (PB5-RX, PB6-TX)
    // PB5 - RX
    GPIOB->MODER &= ~GPIO_MODER_MODE5;
    GPIOB->MODER |= GPIO_MODER_MODE5_1;
    GPIOB->AFR[0] &= ~GPIO_AFRL_AFSEL5;
    GPIOB->AFR[0] |= (9u << GPIO_AFRL_AFSEL5_Pos);
    GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD5;
    GPIOB->PUPDR |= GPIO_PUPDR_PUPD5_0;
    GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR5;

    // PB6 - TX
    GPIOB->MODER &= ~GPIO_MODER_MODE6;
    GPIOB->MODER |= GPIO_MODER_MODE6_1;
    GPIOB->AFR[0] &= ~GPIO_AFRL_AFSEL6;
    GPIOB->AFR[0] |= (9u << GPIO_AFRL_AFSEL6_Pos);
    GPIOB->OTYPER &= ~GPIO_OTYPER_OT6;
    GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR6;

    // 3. Вход в режим инициализации CAN2
    CAN2->MCR |= CAN_MCR_INRQ;
    timeout = get_tick_ms();
    while (!(CAN2->MSR & CAN_MSR_INAK)) {
        if ((get_tick_ms() - timeout) > CAN_TIMEOUT_MS) {
            uart_send_line("CAN2 init timeout!");
            return;
        }
    }

    // 4. Настройка CAN2
    CAN2->MCR |= CAN_MCR_NART;
    CAN2->MCR |= CAN_MCR_AWUM;

    // 5. Настройка битовой синхронизации
    uint32_t prescaler = 6;
    uint32_t ts1 = 11;
    uint32_t ts2 = 2;

    CAN2->BTR = 0;
    CAN2->BTR &= ~(CAN_BTR_SILM | CAN_BTR_LBKM);
    CAN2->BTR |= ((prescaler - 1) << CAN_BTR_BRP_Pos);
    CAN2->BTR |= ((ts1-1) << CAN_BTR_TS1_Pos);
    CAN2->BTR |= ((ts2-1) << CAN_BTR_TS2_Pos);

    uart_printf_line("CAN2 BTR: BRP=%d, TS1=%d, TS2=%d", prescaler-1, ts1-1, ts2-1);

    // ============================================
    // НАСТРОЙКА ФИЛЬТРОВ В РЕЖИМЕ LIST MODE
    // ============================================

    // Сначала инициализируем CAN1 (нужен для работы фильтров)
    CAN1->MCR |= CAN_MCR_INRQ;
    timeout = get_tick_ms();
    while (!(CAN1->MSR & CAN_MSR_INAK)) {
        if ((get_tick_ms() - timeout) > CAN_TIMEOUT_MS) {
            uart_send_line("CAN1 exit init timeout!");
            return;
        }
    }

    CAN1->FMR |= CAN_FMR_FINIT;
    CAN1->FA1R = 0;

    // Используем фильтры 14 и 15 в List mode
    CAN1->FM1R |= (CAN_FM1R_FBM14 | CAN_FM1R_FBM15);

    // 16-битный режим
    CAN1->FS1R &= ~(CAN_FS1R_FSC14 | CAN_FS1R_FSC15);

    // Назначаем на FIFO0
    CAN1->FFA1R &= ~(CAN_FFA1R_FFA14 | CAN_FFA1R_FFA15);

    // ID для фильтров (сдвиг на 5 бит)
    CAN1->sFilterRegister[14].FR1 = (FRAME_1_ID << 5);
    CAN1->sFilterRegister[14].FR2 = 0;

    CAN1->sFilterRegister[15].FR1 = (FRAME_2_ID << 5);
    CAN1->sFilterRegister[15].FR2 = 0;

    // Активация фильтров
    CAN1->FA1R |= (1 << 14) | (1 << 15);

    CAN1->FMR &= ~CAN_FMR_FINIT;

    // Выход из режима инициализации CAN1
    CAN1->MCR &= ~CAN_MCR_INRQ;
    timeout = get_tick_ms();
    while ((CAN1->MSR & CAN_MSR_INAK) != 0) {
        if ((get_tick_ms() - timeout) > CAN_TIMEOUT_MS) {
            uart_send_line("CAN1 exit init timeout!");
            return;
        }
    }

    // 7. Выход из режима инициализации CAN2
    CAN2->MCR &= ~(CAN_MCR_INRQ);
    timeout = get_tick_ms();
    while ((CAN2->MSR & CAN_MSR_INAK) != 0) {
        if ((get_tick_ms() - timeout) > CAN_TIMEOUT_MS) {
            uart_send_line("CAN2 exit init timeout!");
            return;
        }
    }

    uart_printf_line("CAN2 initialized with List Mode: IDs 0x%03X and 0x%03X", 
        FRAME_1_ID, 
        FRAME_2_ID
    );
}

can_error_t can_2_transmit_msg(can_message_t message)
{
    uint32_t timeout;
    
    // Проверка параметров
    if (message.dlc > 8) {
        return CAN_ERROR_PARAM;
    }
    
    // Проверяем, есть ли свободный mailbox
    if ((CAN2->TSR & CAN_TSR_TME0) == 0) {
        return CAN_ERROR_MAILBOX_BUSY;
    }
    
    // Очищаем mailbox
    CAN2->sTxMailBox[0].TIR = 0;
    CAN2->sTxMailBox[0].TDTR = 0;
    CAN2->sTxMailBox[0].TDLR = 0;
    CAN2->sTxMailBox[0].TDHR = 0;
    
    // Настройка идентификатора (стандартный ID)
    CAN2->sTxMailBox[0].TIR &= ~CAN_TI0R_IDE;     // Standard ID
    CAN2->sTxMailBox[0].TIR &= ~CAN_TI0R_RTR;     // Data frame
    CAN2->sTxMailBox[0].TIR |= (message.id << CAN_TI0R_STID_Pos);
    
    // Настройка длины данных
    CAN2->sTxMailBox[0].TDTR = (message.dlc & 0x0F);
    
    // Заполнение данных
    for (uint16_t i = 0; i < message.dlc; i++) {
        if (i < 4) {
            CAN2->sTxMailBox[0].TDLR |= (message.data[i] << (8 * i));
        } else {
            CAN2->sTxMailBox[0].TDHR |= (message.data[i] << (8 * (i - 4)));
        }
    }
    
    // Запуск передачи
    CAN2->sTxMailBox[0].TIR |= CAN_TI0R_TXRQ;
    
    // Ожидание завершения передачи
    timeout = get_tick_ms();
    while ((CAN2->TSR & CAN_TSR_RQCP0) == 0) {
        if ((get_tick_ms() - timeout) > CAN_TIMEOUT_MS) {
            return CAN_ERROR_TIMEOUT;
        }
    }
    
    // Проверка результата передачи
    if (CAN2->TSR & CAN_TSR_TXOK0) {
        CAN2->TSR |= CAN_TSR_RQCP0;  // Clear flag
        return CAN_OK;
    }
    
    // Ошибка передачи
    uint8_t error_code = (CAN2->ESR & CAN_ESR_LEC) >> CAN_ESR_LEC_Pos;
    CAN2->TSR |= CAN_TSR_RQCP0;  // Clear flag

    bool is_transmission_ok = (CAN2->TSR & CAN_TSR_TXOK0);
    bool is_arbiration_lost = (CAN2->TSR & CAN_TSR_ALST0);
    bool is_transmission_failed = (CAN2->TSR & CAN_TSR_TERR0);
    uart_printf_line(
        "CAN transmit error! is_transmission_ok = %d, is_arbiration_lost = %d, is_transmission_failed = %d",
        is_transmission_ok,
        is_arbiration_lost,
        is_transmission_failed
    );
    
    uart_printf_line("CAN transmit error: LEC=%d", error_code);
    
    return CAN_ERROR_TX_FAILED;
}

can_error_t can_2_receive_msg(can_message_t *message)
{
    // Проверка валидности указателя на выходную структуру
    if (message == NULL) {
        return CAN_ERROR_PARAM;
    }

    // Проверка наличия сообщения в FIFO0
    if ((CAN2->RF0R & CAN_RF0R_FMP0) == 0) {
        return CAN_ERROR_NOMESSAGE;
    }

    // Отладочный вывод: содержимое RIR регистра
    uint32_t rir = CAN2->sFIFOMailBox[0].RIR;

    // Извлечение ID (для стандартного фрейма)
    message->id = (rir >> CAN_RI0R_STID_Pos) & 0x7FF;

    // Извлечение DLC (длины данных)
    message->dlc = CAN2->sFIFOMailBox[0].RDTR & CAN_RDT0R_DLC;

    // Проверка RTR бита
    message->is_remote = ((rir & CAN_RI0R_RTR) != 0);

    // Проверка Extented ID бита
    message->is_extended = ((rir & CAN_RI0R_IDE) != 0);

    // Обнуляем данные для Remote Frame
    if (message->is_remote) {
        memset(message->data, 0, 8);
    } else {
        // Чтение данных только для Data Frame
        uint32_t rdlr = CAN2->sFIFOMailBox[0].RDLR;
        uint32_t rdhr = CAN2->sFIFOMailBox[0].RDHR;

        for (uint16_t i = 0; i < message->dlc && i < 8; i++) {
            if (i < 4) {
                message->data[i] = (rdlr >> (8 * i)) & 0xFF;
            } else {
                message->data[i] = (rdhr >> (8 * (i - 4))) & 0xFF;
            }
        }
    }

    // Отладочный вывод
    uart_printf_line("CAN RECV: ID=0x%03X, DLC=%d, REMOTE=%d, EXTENDED_ID=%d",
        message->id,
        message->dlc,
        message->is_remote,
        message->is_extended
    );
    
    if (message->is_remote) {
        uart_send_line("  -> This is a REMOTE FRAME!");
    }

    // Освобождение FIFO
    CAN2->RF0R |= CAN_RF0R_RFOM0;

    return CAN_OK;  // OK
}

const char* can_2_error_to_string(can_error_t error)
{
    switch (error) {
        case CAN_OK: return "OK";
        case CAN_ERROR_BUSY: return "Busy";
        case CAN_ERROR_TIMEOUT: return "Timeout";
        case CAN_ERROR_PARAM: return "Invalid parameter";
        case CAN_ERROR_NOMESSAGE: return "No message";
        case CAN_ERROR_TX_FAILED: return "Transmission failed";
        case CAN_ERROR_FIFO_EMPTY: return "FIFO empty";
        case CAN_ERROR_FIFO_FULL: return "FIFO full";
        case CAN_ERROR_MAILBOX_BUSY: return "Mailbox busy";
        default:
            return "Unknown error";
    }
}