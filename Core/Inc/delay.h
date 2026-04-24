#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

#define SYSTEM_TIMER_MAX_VALUE  0xFFFFFF

typedef enum {
    RUNNING_LEDS_FROM_AT24C02_EVENT = 1,
    RUNNING_LEDS_FROM_W25Q64_EVENT = 2,
} event_id_t;

event_id_t get_event_id(void);

void delay_init(uint32_t frequency_hz);
void delay_ms(uint32_t milliseconds);
uint32_t get_tick_ms(void);

#endif // DELAY_H