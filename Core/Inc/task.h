#ifndef TASK_H
#define TASK_H

#include "led.h"

void save_pressed_button_into_eeprom(led_id_t led_id);
void runnig_leds_from_eeprom(void);
void clear_eeprom(void);
void test_w25q64(void);

#endif // TASK_H