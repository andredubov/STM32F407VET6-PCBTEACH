// timer_capture.h
#ifndef TIMER_CAPTURE_H
#define TIMER_CAPTURE_H

#include <stdint.h>
#include <stdbool.h>

// Переменная для отслеживания интервала в режиме отладки
// Измеряется в секундах с точностью 1 мс
extern volatile uint32_t capture;

// Функции инициализации
void timer1_init_in_capture_mode(void);
void timer2_init_as_prescaler(void);

// Управление таймерами
void timer2_start(void);
void timer2_stop(void);
void timer1_start(void);
void timer1_stop(void);

// Сброс счетчиков таймеров
void timer2_reset_counter(void);
void timer1_reset_counter(void);

// Получить измеренный интервал в секундах (с плавающей точкой)
float get_interval_seconds(void);

// Получить измеренный интервал в миллисекундах
uint32_t get_interval_ms(void);

// Проверить, было ли измерение завершено
bool is_measurement_complete(void);

// Сбросить измерение для следующей пары нажатий
void reset_measurement(void);

void test_timer2(void);
void test_timer1_ticking(void);
void test_all_itr_sources(void);
void test_timer1_internal(void);
void test_timer1_counting(void);
void diagnose_timer_connection(void);
void test_timer2_output(void);
void debug_tim1_counting(void);
void debug_tim1_capture_pins(void);

#endif // TIMER_CAPTURE_H