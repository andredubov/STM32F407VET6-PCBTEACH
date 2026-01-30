#include "stm32f407xx.h"

void rcc_init(void) 
{
    // 1. Включить HSI как временный источник
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {
        __NOP();
    }

    // 2. Включить HSE (внешний кварц 25 МГц)
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) {
        __NOP();
    }

    // 3. Отключить PLL перед настройкой
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY) {
        __NOP();
    }

    // 4. Настройка PLL для 84 МГц из 25 МГц HSE
    //    HSE = 25 МГц
    //    M = 25, N = 336, P = 4
    //    VCO = (25 МГц / 25) * 336 = 336 МГц
    //    PLL_OUT = 336 МГц / 4 = 84 МГц

    RCC->PLLCFGR = 0;
    RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSE;  // Источник HSE

    // PLLM = 25 (делитель входа)
    // Диапазон: 2-63
    RCC->PLLCFGR |= (25 << 0);  // 25 МГц / 25 = 1 МГц на вход VCO

    // PLLN = 336 (умножитель VCO)
    // Диапазон: 192-432
    // VCO = 1 МГц * 336 = 336 МГц (в пределах 100-432 МГц)
    RCC->PLLCFGR |= (336 << 6);

    // PLLP = 4 (делитель выхода) - биты 17:16 = 01
    // 336 МГц / 4 = 84 МГц
    RCC->PLLCFGR &= ~(3 << 16);  // Сбросить биты 17:16
    RCC->PLLCFGR |= (1 << 16);   // Установить 01 (деление на 4)
    
    // PLLQ = 7 (для USB, SDIO, RNG)
    // 336 МГц / 7 ≈ 48 МГц (требуется для USB)
    RCC->PLLCFGR |= (7 << 24);
    
    // 5. Настройка делителей шин
    RCC->CFGR = 0;
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;    // AHB = 84 МГц
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;   // APB1 = 21 МГц (макс 42 МГц)
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;   // APB2 = 42 МГц (макс 84 МГц)
    
    // 6. Настройка Flash для 84 МГц
    // При 2.7-3.6V и 84 МГц нужно 2 wait states
    FLASH->ACR = 0;
    FLASH->ACR |= FLASH_ACR_LATENCY_2WS;
    FLASH->ACR |= FLASH_ACR_PRFTEN;
    FLASH->ACR |= FLASH_ACR_ICEN;
    FLASH->ACR |= FLASH_ACR_DCEN;
    
    // 7. Включить PLL
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {
        __NOP();
    }
    
    // 8. Переключить системную частоту на PLL
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
        __NOP();
    }
}