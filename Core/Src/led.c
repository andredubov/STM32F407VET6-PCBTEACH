#include <stdint.h>
#include "stm32f4xx.h"
#include "delay.h"
#include "led.h"
#include "at24c02.h"

void leds_init(void)
{
    // Включить тактирование порта E (не сбрасывая другие биты!)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;

    // LED1 (PE13)
    GPIOE->MODER &= ~(GPIO_MODER_MODER13);           // сбросить режим
    GPIOE->MODER |= GPIO_MODER_MODER13_0;            // установить вывод (01)
    GPIOE->OTYPER &= ~(GPIO_OTYPER_OT_13);           // push-pull (0)
    GPIOE->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR13);    // низкая скорость
    GPIOE->PUPDR &= ~(GPIO_PUPDR_PUPDR13);           // без подтяжки

    // LED2 (PE14)
    GPIOE->MODER &= ~(GPIO_MODER_MODER14);
    GPIOE->MODER |= GPIO_MODER_MODER14_0;
    GPIOE->OTYPER &= ~(GPIO_OTYPER_OT_14);
    GPIOE->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR14);
    GPIOE->PUPDR &= ~(GPIO_PUPDR_PUPDR14);

    // LED3 (PE15)
    GPIOE->MODER &= ~(GPIO_MODER_MODER15);
    GPIOE->MODER |= GPIO_MODER_MODER15_0;
    GPIOE->OTYPER &= ~(GPIO_OTYPER_OT_15);
    GPIOE->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR15);
    GPIOE->PUPDR &= ~(GPIO_PUPDR_PUPDR15);

    // Выключить все светодиоды (0 = включено, 1 = выключено)
    GPIOE->BSRR = GPIO_BSRR_BS_13 | GPIO_BSRR_BS_14 | GPIO_BSRR_BS_15;
}

void led_toggle(led_id_t led_id) 
{
    switch (led_id) {
        case LED_1:
            GPIOE->ODR ^= (1 << LED_1);
            break;
        case LED_2:
            GPIOE->ODR ^= (1 << LED_2);
            break;
        case LED_3:
            GPIOE->ODR ^= (1 << LED_3);
            break;
        default:
            break;
    }
}

void led_on(led_id_t led_id) 
{
    switch (led_id) {
        case LED_1:
            GPIOE->BSRR |= GPIO_BSRR_BR13;
            break;
        case LED_2:
            GPIOE->BSRR |= GPIO_BSRR_BR14;
            break;
        case LED_3:
            GPIOE->BSRR |= GPIO_BSRR_BR15;
            break;
        default:
            break;
    }
}

void led_off(led_id_t led_id) 
{
    switch (led_id) {
        case LED_1:
            GPIOE->BSRR |= GPIO_BSRR_BS13;
            break;
        case LED_2:
            GPIOE->BSRR |= GPIO_BSRR_BS14;
            break;
        case LED_3:
            GPIOE->BSRR |= GPIO_BSRR_BS15;
            break;
        default:
            break;
    }
}

void runnig_leds_1(uint32_t repeat_cnt, uint32_t delay)
{
    for (uint32_t i = 0;  i < repeat_cnt; ++i)
    {
        for (uint8_t led_index = 0; led_index < 3; ++led_index) 
        {
            switch (led_index) {
                case 0:
                    GPIOE->BSRR |= GPIO_BSRR_BR13;
                    GPIOE->BSRR |= GPIO_BSRR_BS14;
                    GPIOE->BSRR |= GPIO_BSRR_BS15;
                    break;
                case 1:
                    GPIOE->BSRR |= GPIO_BSRR_BS13;
                    GPIOE->BSRR |= GPIO_BSRR_BR14;
                    GPIOE->BSRR |= GPIO_BSRR_BS15;                    
                    break;
                case 2:
                    GPIOE->BSRR |= GPIO_BSRR_BS13;
                    GPIOE->BSRR |= GPIO_BSRR_BS14;
                    GPIOE->BSRR |= GPIO_BSRR_BR15;
                    break;
            }

            delay_ms(delay);
        }
    }
}

void runnig_leds_2(uint32_t repeat_cnt, uint32_t delay)
{
    for (uint32_t i = 0;  i < repeat_cnt; ++i)
    {
        for (uint8_t state_id = 0; state_id < 4; ++state_id)
        {
            switch (state_id) {
                case 0:
                    GPIOE->BSRR |= GPIO_BSRR_BR13;
                    GPIOE->BSRR |= GPIO_BSRR_BS14;
                    GPIOE->BSRR |= GPIO_BSRR_BS15;                                    
                    break;
                case 1:
                    GPIOE->BSRR |= GPIO_BSRR_BR13;
                    GPIOE->BSRR |= GPIO_BSRR_BR14;
                    GPIOE->BSRR |= GPIO_BSRR_BS15;
                    break;
                case 2:
                    GPIOE->BSRR |= GPIO_BSRR_BR13;
                    GPIOE->BSRR |= GPIO_BSRR_BR14;
                    GPIOE->BSRR |= GPIO_BSRR_BR15;
                    break;
                case 3:
                    GPIOE->BSRR |= GPIO_BSRR_BR13;
                    GPIOE->BSRR |= GPIO_BSRR_BR14;
                    GPIOE->BSRR |= GPIO_BSRR_BS15;
                    break;
            }

            delay_ms(delay);
        }
    }
}

void runnig_leds_3(uint32_t repeat_cnt, uint32_t delay)
{
    for (uint32_t i = 0;  i < repeat_cnt; ++i)
    {
        for (uint8_t state_id = 0; state_id < 2; ++state_id) 
        {
            switch (state_id) {
                case 0:
                    GPIOE->BSRR |= GPIO_BSRR_BR13;
                    GPIOE->BSRR |= GPIO_BSRR_BR14;
                    GPIOE->BSRR |= GPIO_BSRR_BR15;
                    break;
                case 1:
                    GPIOE->BSRR |= GPIO_BSRR_BS13;
                    GPIOE->BSRR |= GPIO_BSRR_BS14;
                    GPIOE->BSRR |= GPIO_BSRR_BS15;
                    break;
            }

            delay_ms(delay);
        }
    }
}