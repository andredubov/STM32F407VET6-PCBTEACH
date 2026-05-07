#include "stm32f407xx.h"
#include "button.h"
#include "delay.h"
#include "critical_section.h"

// Константы для демпфирования
#define DEBOUNCE_COUNTER_MAX 5      // количество проверок для подтверждения состояния (при вызове обработчика каждые 50 мс = 50 мс)

volatile button_event_t button_event = BUTTON_NONE;
volatile button_debounce_t button_pressed_debounce = {0};
volatile button_debounce_t button_released_debounce = {0};
volatile uint8_t button_raw_state[3] = {0};  // сырое состояние из прерывания

button_event_t get_button_event(void)
{
    CRITICAL_SECTION_START();
    button_event_t event = button_event;
    button_event = BUTTON_NONE; // Сбрасываем событие после чтения
    CRITICAL_SECTION_END();

    return event;
}

// Обработчик прерывания - только фиксирует сырое состояние
void EXTI15_10_IRQHandler(void)
{
    uint32_t pr = EXTI->PR;

    // Используем BASEPRI для быстрой защиты (не отключаем SysTick)
    uint32_t basepri = critical_enter_basp();

    if (pr & EXTI_PR_PR10) {
        // Читаем реальное состояние пина
        button_raw_state[0] = !(GPIOE->IDR & GPIO_IDR_IDR_10);
        EXTI->PR = EXTI_PR_PR10;
    }
    
    if (pr & EXTI_PR_PR11) {
        button_raw_state[1] = !(GPIOE->IDR & GPIO_IDR_IDR_11);
        EXTI->PR = EXTI_PR_PR11;
    }
    
    if (pr & EXTI_PR_PR12) {
        button_raw_state[2] = !(GPIOE->IDR & GPIO_IDR_IDR_12);
        EXTI->PR = EXTI_PR_PR12;
    }

    critical_exit_basp(basepri);
}

// Обработчик демпфирования - вызывать периодически (например, каждые 10 мс из таймера)
void buttons_debounce_handler(void)
{
    uint32_t basepri = critical_enter_basp();
    
    for (int i = 0; i < 3; i++) 
    {
        // Текущее сырое состояние кнопки (нажата = 1, отпущена = 0)
        uint8_t raw = button_raw_state[i];
        
        // === ОБРАБОТКА НАЖАТИЯ ===
        if (raw) {
            if (button_pressed_debounce.debounce_counter[i] < DEBOUNCE_COUNTER_MAX) {
                button_pressed_debounce.debounce_counter[i]++;
                
                if (button_pressed_debounce.debounce_counter[i] >= DEBOUNCE_COUNTER_MAX) {
                    // Дребезг прошел - состояние стабилизировалось
                    if (!button_pressed_debounce.button_state[i]) {
                        // Кнопка была отпущена, а теперь нажата
                        button_pressed_debounce.button_state[i] = 1;
                        
                        // Генерируем событие НАЖАТИЯ
                        switch (i) {
                            case 0: button_event = BUTTON_S1_PRESSED; break;
                            case 1: button_event = BUTTON_S2_PRESSED; break;
                            case 2: button_event = BUTTON_S3_PRESSED; break;
                        }
                    }
                }
            }
        } else {
            // === ОБРАБОТКА ОТПУСКАНИЯ ===
            if (button_released_debounce.debounce_counter[i] < DEBOUNCE_COUNTER_MAX) {
                button_released_debounce.debounce_counter[i]++;
                
                if (button_released_debounce.debounce_counter[i] >= DEBOUNCE_COUNTER_MAX) {
                    // Дребезг прошел - состояние стабилизировалось
                    if (button_pressed_debounce.button_state[i]) {
                        // Кнопка была нажата, а теперь отпущена
                        button_pressed_debounce.button_state[i] = 0;
                        
                        // Генерируем событие ОТПУСКАНИЯ
                        switch (i) {
                            case 0: button_event = BUTTON_S1_RELEASED; break;
                            case 1: button_event = BUTTON_S2_RELEASED; break;
                            case 2: button_event = BUTTON_S3_RELEASED; break;
                        }
                    }
                }
            }
        }
        
        // Сброс счетчиков при изменении сырого состояния
        // Если кнопка нажата, но был дребезг (raw=0), сбрасываем счетчик нажатия
        if (!raw && button_pressed_debounce.debounce_counter[i] > 0) {
            button_pressed_debounce.debounce_counter[i] = 0;
        }
        
        // Если кнопка отпущена, но был дребезг (raw=1), сбрасываем счетчик отпускания
        if (raw && button_released_debounce.debounce_counter[i] > 0) {
            button_released_debounce.debounce_counter[i] = 0;
        }
    }
    
    critical_exit_basp(basepri);
}

void buttons_init(void)
{
    // 1. Включить тактирование порта E
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;

    // 2. Настроить пины на вход
    GPIOE->MODER &= ~(GPIO_MODER_MODER10 | GPIO_MODER_MODER11 | GPIO_MODER_MODER12);

    // 3. Включить внутренний pull-up
    GPIOE->PUPDR &= ~(GPIO_PUPDR_PUPDR10 | GPIO_PUPDR_PUPDR11 | GPIO_PUPDR_PUPDR12);
    GPIOE->PUPDR |= (GPIO_PUPDR_PUPDR10_0 | GPIO_PUPDR_PUPDR11_0 | GPIO_PUPDR_PUPDR12_0);

    // 4. Включить тактирование SysCfg
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    // 5. Настроить EXTI линии
    // Для PE10 (EXTI10)
    SYSCFG->EXTICR[2] &= ~(SYSCFG_EXTICR3_EXTI10);
    SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI10_PE;

    // Для PE11 (EXTI11)
    SYSCFG->EXTICR[2] &= ~(SYSCFG_EXTICR3_EXTI11);
    SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI11_PE;

    // Для PE12 (EXTI12)
    SYSCFG->EXTICR[3] &= ~SYSCFG_EXTICR4_EXTI12;
    SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI12_PE;

    // 6. Настроить маску прерываний
    EXTI->IMR = (EXTI_IMR_MR10 | EXTI_IMR_MR11 | EXTI_IMR_MR12);

    // 7. Настроить триггер по фронту и срезу (falling edge 1->0 and rasing edge 0->1)
    EXTI->FTSR |= (EXTI_FTSR_TR10 | EXTI_FTSR_TR11 | EXTI_FTSR_TR12);
    EXTI->RTSR |= (EXTI_RTSR_TR10 | EXTI_RTSR_TR11 | EXTI_RTSR_TR12);

    // 8. Настроить NVIC
    NVIC_SetPriority(EXTI15_10_IRQn, 0x0B);  // Средний приоритет
    NVIC_EnableIRQ(EXTI15_10_IRQn);

    CRITICAL_SECTION_START();
    // 9. Инициализация структур демпфирования
    for (int i = 0; i < 3; i++) {
        button_pressed_debounce.debounce_counter[i] = 0;
        button_pressed_debounce.button_state[i] = 0;
        button_pressed_debounce.button_pressed_flag[i] = 0;

        button_released_debounce.debounce_counter[i] = 0;
        button_released_debounce.button_state[i] = 0;
        button_released_debounce.button_pressed_flag[i] = 0;

        button_raw_state[i] = 0;
    }
    button_event = BUTTON_NONE;
    CRITICAL_SECTION_END();
}