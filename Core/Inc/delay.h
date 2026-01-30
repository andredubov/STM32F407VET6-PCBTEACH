#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

void delay_init(uint32_t frequency_khz);
void delay_ms(uint32_t milliseconds);
uint32_t get_tick(void);

#endif // DELAY_H