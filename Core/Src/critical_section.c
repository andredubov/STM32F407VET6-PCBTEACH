#include "critical_section.h"

uint32_t critical_enter(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();  // Отключаем все прерывания
    return primask;
}

void critical_exit(uint32_t primask)
{
    if (primask == 0) {
        __enable_irq();  // Восстанавливаем только если прерывания были включены
    }
}

uint32_t critical_enter_basp(void)
{
    uint32_t basepri = __get_BASEPRI();
    // Отключаем прерывания с приоритетом >= 0x0A (10)
    // (оставляем только самые высокоприоритетные - SysTick и фатальные ошибки)
    __set_BASEPRI(0x0A << (8 - __NVIC_PRIO_BITS));
    return basepri;
}

void critical_exit_basp(uint32_t basepri)
{
    __set_BASEPRI(basepri);
}