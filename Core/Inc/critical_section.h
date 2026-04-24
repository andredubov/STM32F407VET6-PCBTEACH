#ifndef CRITICAL_SECTION_H
#define CRITICAL_SECTION_H

#include <stdint.h>
#include "stm32f407xx.h"

// Сохраняет состояние PRIMASK и отключает прерывания
uint32_t critical_enter(void);

// Восстанавливает состояние PRIMASK
void critical_exit(uint32_t primask);

// Блокировка только для прерываний с указанным приоритетом и ниже
// (использует BASEPRI вместо PRIMASK)
uint32_t critical_enter_basp(void);
void critical_exit_basp(uint32_t basepri);

// Макросы для удобства использования
#define CRITICAL_SECTION_START() uint32_t __primask = critical_enter()
#define CRITICAL_SECTION_END()   critical_exit(__primask)

#define CRITICAL_BASP_START()    uint32_t __basepri = critical_enter_basp()
#define CRITICAL_BASP_END()      critical_exit_basp(__basepri)

#endif // CRITICAL_SECTION_H