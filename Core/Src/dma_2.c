#include <stdbool.h>
#include "stm32f4xx.h"
#include "critical_section.h"

volatile static bool is_transfer_completed = false;
volatile static bool is_transfer_failed = false;

void DMA2_MemToMem_Init(uint8_t *src, uint8_t *dst , uint32_t length)
{
    // 1. Включаем тактирование DMA2
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    
    // 2. Отключаем канал (Stream0) перед настройкой
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    
    // 3. Ждем подтверждения отключения (опционально)
    while(DMA2_Stream0->CR & DMA_SxCR_EN);
    
    // 4. Настройка регистра CR (Control Register)
    uint32_t cr = 0;
    
    // Направление: чтение из памяти в память
    cr &= ~DMA_SxCR_DIR;            // Сброс обоих битов
    cr |= DMA_SxCR_DIR_1;           // Установка DIR = 10 (память-память)
    
    // Размер данных: byte (8-bit)
    cr &= ~DMA_SxCR_MSIZE;         // MSIZE = 00 (8 bits)
    cr &= ~DMA_SxCR_PSIZE;         // PSIZE = 00 (8 bits)
    
    // Инкремент адреса для источника и приемника
    cr |= DMA_SxCR_MINC;           // Increment memory pointer
    cr |= DMA_SxCR_PINC;           // Increment peripheral pointer
    
    // Циркулярный режим не используем (однократная передача)
    cr &= ~DMA_SxCR_CIRC;
    
    // Приоритет: высокий
    cr &= ~DMA_SxCR_PL;           // PL = 10 (High priority)
    cr |= DMA_SxCR_PL_1;
    
    // Прерывания (опционально)
    cr |= DMA_SxCR_TCIE;           // Включить прерывание по завершению передачи
    cr |= DMA_SxCR_TEIE;        // Включить прерывание по ошибке
    
    // Применяем настройки
    DMA2_Stream0->CR = cr;
    
    // 5. Настройка адресов и количества данных
    DMA2_Stream0->PAR = (uint32_t)src;   // Адрес источника (PINC работает как M2M)
    DMA2_Stream0->M0AR = (uint32_t)dst;  // Адрес приемника
    DMA2_Stream0->NDTR = length;             // Количество элементов для передачи
    
    // 6. Настройка прерываний в NVIC (если используете прерывания)
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    NVIC_SetPriority(DMA2_Stream0_IRQn, 0x0C);

    CRITICAL_SECTION_START();    
    is_transfer_completed = false;
    is_transfer_failed = false;
    CRITICAL_SECTION_END();
}

// Функция запуска передачи
void DMA2_StartTransfer(void)
{
    CRITICAL_SECTION_START();
    is_transfer_completed = false;
    is_transfer_failed = false;
    CRITICAL_SECTION_END();

    // Сбрасываем флаги перед запуском
    DMA2->LIFCR = DMA_LIFCR_CTCIF0 | DMA_LIFCR_CTEIF0 | DMA_LIFCR_CHTIF0;
    
    // Включаем DMA Stream
    DMA2_Stream0->CR |= DMA_SxCR_EN;
}

bool DMA2_IsTransferComplete(void)
{
    bool result;

    CRITICAL_SECTION_START();
    result = is_transfer_completed && !is_transfer_failed;
    CRITICAL_SECTION_END();

    return result;
}

// Дополнительная функция для проверки ошибки
bool DMA2_IsTransferFailed(void)
{
    bool result;
    
    CRITICAL_SECTION_START();
    result = is_transfer_failed;
    CRITICAL_SECTION_END();
    
    return result;
}

// Обработчик прерывания DMA2 Stream0
void DMA2_Stream0_IRQHandler(void)
{
    // Проверяем флаг завершения передачи
    if (DMA2->LISR & DMA_LISR_TCIF0)
    {        
        // Сбрасываем флаг
        DMA2->LIFCR = DMA_LIFCR_CTCIF0;

        uint32_t basepri = critical_enter_basp();

        is_transfer_completed = true;

        critical_exit_basp(basepri);

        // Здесь можно добавить код, который выполнится после завершения копирования
        // Например, установить флаг или вызвать callback-функцию
    }
    
    // Проверка на ошибку
    if (DMA2->LISR & DMA_LISR_TEIF0)
    {
        // Сбрасываем флаг ошибки
        DMA2->LIFCR = DMA_LIFCR_CTEIF0;

        uint32_t basepri = critical_enter_basp();

        is_transfer_failed = true;

        critical_exit_basp(basepri);
        
        // Обработка ошибки...
    }
}