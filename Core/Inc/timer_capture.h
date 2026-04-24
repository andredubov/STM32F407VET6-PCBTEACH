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
void timer1_start(void);
void timer2_start(void);
void timer1_stop(void);
void timer2_stop(void);

// Получить измеренный интервал в секундах (с плавающей точкой)
float get_interval_seconds(void);

// Получить измеренный интервал в миллисекундах
uint32_t get_interval_ms(void);

// Проверить, было ли измерение завершено
bool is_time_measurement_completed(void);

// Сбросить измерение для следующей пары нажатий
void reset_time_measurement(void);

#endif // TIMER_CAPTURE_H