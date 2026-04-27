#ifndef ADC_H
#define ADC_H

#include <stdbool.h>
#include <stdint.h>

// События ADC
typedef enum {
    ADC_EVENT_NONE = 0,
    ADC_EVENT_WATCHDOG_NORMAL,        // Возврат в нормальный диапазон
    ADC_EVENT_CONVERSION_COMPLETE,    // Завершение преобразования (опционально)
    ADC_EVENT_WATCHDOG_TRIGGERED,     // Выход за пределы [10%...80%]
    ADC_EVENT_HIGH_THRESHOLD,         // Превышение верхнего порога (опционально)
    ADC_EVENT_LOW_THRESHOLD,          // Падение ниже нижнего порога (опционально)    
} adc_event_t;

void adc_init(void);
void adc_start(void);
void adc_stop(void);

bool adc_is_watchdog_triggered(void);
void adc_clear_watchdog_flag(void);

adc_event_t get_adc_event(void);

#endif // ADC_H