#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

typedef enum {
    NONE = 0,
    BUTTON_1_PRESSED,
    BUTTON_2_PRESSED,
    BUTTON_3_PRESSED,
} ButtonEvent_t;

typedef struct {
    uint8_t debounce_counter[3];
    uint8_t button_state[3];
    uint8_t button_pressed_flag[3];
} ButtonDebounce_t;

void buttons_init(void);
ButtonEvent_t get_button_event(void);
void buttons_debounce_handler(void);  // Объявление функции

#endif  // BUTTON_H