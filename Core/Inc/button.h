#ifndef BUTTON_H
#define BUTTON_H

#define DEBOUNCE_TIME_MS     10     // время демпфирования в мс

#include <stdint.h>

typedef enum {
    NONE = 0,
    BUTTON_1_PRESSED,
    BUTTON_2_PRESSED,
    BUTTON_3_PRESSED,
} button_event_t;

typedef struct {
    uint8_t debounce_counter[3];
    uint8_t button_state[3];
    uint8_t button_pressed_flag[3];
} button_debounce_t;

void buttons_init(void);
button_event_t get_button_event(void);
void buttons_debounce_handler(void);  // Объявление функции

#endif  // BUTTON_H