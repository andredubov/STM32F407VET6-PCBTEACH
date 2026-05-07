#ifndef BUTTON_H
#define BUTTON_H

#define DEBOUNCE_TIME_MS     10     // время демпфирования в мс

#include <stdint.h>

typedef enum {
    BUTTON_NONE = 0,
    BUTTON_S1_PRESSED,
    BUTTON_S2_PRESSED,
    BUTTON_S3_PRESSED,
    BUTTON_S1_RELEASED,
    BUTTON_S2_RELEASED,
    BUTTON_S3_RELEASED,
} button_event_t;

typedef enum {
    BUTTON_S1 = 1,
    BUTTON_S2,
    BUTTON_S3,
} button_id_t;

typedef struct {
    uint8_t debounce_counter[3];
    uint8_t button_state[3];
    uint8_t button_pressed_flag[3];
} button_debounce_t;

void buttons_init(void);
button_event_t get_button_event(void);
void buttons_debounce_handler(void);  // Объявление функции

#endif  // BUTTON_H