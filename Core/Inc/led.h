#ifndef LED_H
#define LED_H

#include <stdint.h>

typedef enum {
    LED_1 = 15,
    LED_2 = 14,
    LED_3 = 13
} led_id_t;

void leds_init(void);

void led_on(led_id_t led_id);
void led_off(led_id_t led_id);
void led_toggle(led_id_t led_id);

void runnnig_leds_1(uint32_t repeat_cnt, uint32_t delay);
void runnnig_leds_2(uint32_t repeat_cnt, uint32_t delay);
void runnnig_leds_3(uint32_t repeat_cnt, uint32_t delay);

#endif // LED_H