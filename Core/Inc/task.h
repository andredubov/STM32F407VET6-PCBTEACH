#ifndef TASK_H
#define TASK_H

#include <stdbool.h>
#include "led.h"

void save_pressed_button_into_eeprom(led_id_t led_id);
void runnig_leds_from_eeprom(void);
void clear_eeprom(void);

void save_led_id_into_eeprom(led_id_t led_id);
void save_leds_ids_into_eeprom(led_id_t led_1_id, led_id_t led_2_id, led_id_t led_3_id);
void load_led_id_from_eeprom(led_id_t led_id);
void switch_on_led();

void start_time_measurement(void);
void get_time_measurement(void);

#endif // TASK_H