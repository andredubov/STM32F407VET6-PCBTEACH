#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

typedef enum {
    RUNNING_LEDS_FROM_EEPROM = 1,
} event_id_t;

event_id_t get_event(void);

void delay_init(uint32_t frequency_hz);
void delay_ms(uint32_t milliseconds);
uint32_t get_tick_ms(void);

#endif // DELAY_H