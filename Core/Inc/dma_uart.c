#include "stm32f407xx.h"

void USART1_DMA_Init(void) {
    // 4. Включение DMA для передатчика USART
    USART1->CR3 |= USART_CR3_DMAT;
}

void DMA_Transfer(uint8_t *buffer, uint16_t size) {
    // Сброс потока перед настройкой
    DMA2_Stream7->CR = 0; 
    while (DMA2_Stream7->CR & DMA_SxCR_EN); // Ждем отключения

    // Очистка флагов прерываний
    DMA2->HIFCR |= DMA_HIFCR_CTCIF7 | DMA_HIFCR_CHTIF7 | DMA_HIFCR_CTEIF7 | DMA_HIFCR_CDTIF7;

    // Настройка адресов
    DMA2_Stream7->PAR = (uint32_t)&(USART1->DR); // Периферия: регистр данных USART
    DMA2_Stream7->M0AR = (uint32_t)buffer;        // Память: адрес буфера
    DMA2_Stream7->NDTR = size;                    // Количество байт

    // Настройка CR:
    // Канал 4, Направление: Memory to Peripheral, Инкремент памяти, 8-битный размер
    DMA2_Stream7->CR = (4 << DMA_SxCR_CHSEL_Pos) | // Channel 4
                       DMA_SxCR_DIR_0 |           // Memory to Peripheral
                       DMA_SxCR_MINC |            // Memory increment
                       DMA_SxCR_TCIE |            // Transfer complete interrupt enable (optional)
                       DMA_SxCR_PL_0;             // Priority: High

    // Включение DMA
    DMA2_Stream7->CR |= DMA_SxCR_EN;
}

uint8_t msg[] = "Hello DMA\r\n";

int main(void) {
    // Инициализация системы (RCC, SystemCoreClock)
    USART1_DMA_Init();
    
    // Передача
    DMA_Transfer(msg, sizeof(msg) - 1);

    while (1) {
        // CPU свободен
    }
}
