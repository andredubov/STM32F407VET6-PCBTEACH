#include <stdbool.h>
#include <string.h>
#include "button.h"
#include "stm32f407xx.h"
#include "led.h"
#include "uart.h"
#include "task.h"
#include "can.h"

#define FRAME_ID_1  0x234
#define FRAME_ID_2  0x432

// Структура для хранения состояния кнопок (для Remote Frame)
static volatile uint8_t last_button_states[3] = {0, 0, 0};

void save_button_event(button_event_t button_event_id)
{
    switch(button_event_id) {
        case BUTTON_S1_PRESSED:
            last_button_states[0] = 1;
            break;
        case BUTTON_S2_PRESSED:
            last_button_states[1] = 1;
            break;
        case BUTTON_S3_PRESSED:
            last_button_states[2] = 1;
            break;
        case BUTTON_S1_RELEASED:
            last_button_states[0] = 0;
            break;
        case BUTTON_S2_RELEASED:
            last_button_states[1] = 0;
            break;
        case BUTTON_S3_RELEASED:
            last_button_states[2] = 0;
            break;
        default:
            break;
    }
}

void send_button_state(void)
{
    can_message_t message;
    can_error_t error;
    
    message.id = FRAME_ID_2;
    message.dlc = 3;
    message.is_remote = false;
    message.is_extended = false;

    // Формируем данные: каждый байт содержит номер нажатой кнопки
    // Формат: [кнопка1, кнопка2, кнопка3]
    // Если кнопка не нажата - 0x00, иначе - номер кнопки (1, 2 или 3)
    message.data[0] = last_button_states[0] ? 0x01 : 0x00;
    message.data[1] = last_button_states[1] ? 0x02 : 0x00;
    message.data[2] = last_button_states[2] ? 0x03 : 0x00;
    
    uart_printf_line("Sending button state: [0x%02X, 0x%02X, 0x%02X]", 
        message.data[0],
        message.data[1],
        message.data[2]
    );

    // Отправляем сообщение с ID = 0x432
    error = can_2_transmit_msg(message);
    
    if (error != CAN_OK) {
        uart_printf_line("Failed to send button state: %s", can_2_error_to_string(error));
    } else {
        uart_send_line("Button state sent successfully");
    }
}

static void switch_on_leds(can_message_t can_message)
{
    if (can_message.dlc > 0) 
    {
        for (uint8_t i = 0; i < LED_MAX; ++i) {
            uint8_t bit_mask = (1 << i);
            led_id_t led_id = LED_1 - i;

            if (can_message.data[0] & bit_mask) {
                led_on(led_id);
            } else {
                led_off(led_id);
            }
        }
    }
}

void can_receive_message(void)
{
    can_error_t error;
    can_message_t message;

    error = can_2_receive_msg(&message);
    if (error != CAN_OK) {
        return;
    }

    if (message.is_remote) {
        // Это Remote Frame - запрос от ПК
        switch(message.id) {
            case FRAME_ID_2:
                send_button_state(); // Отправляем состояние кнопок в ответ
                break;
            default:
                uart_printf_line("Unknown Remote Frame ID: 0x%03X", message.id);            
                break;
        }
    } else {
        switch(message.id) {
            case FRAME_ID_1:
                switch_on_leds(message); // зажигаем соответствующие светодиоды
                break;
            default:
                uart_printf_line("Unknown Data Frame ID: 0x%03X", message.id);            
                break;
        }
    }
}