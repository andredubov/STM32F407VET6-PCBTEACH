#include "main.h"
#include "button.h"
#include "delay.h"

void system_init(void)
{
    uint32_t mcu_frequency_hz = 84000000u;

    rcc_init();
    delay_init(mcu_frequency_hz);
    leds_init();
    buttons_init();
    uart_init();
    can_2_init();
}

int main(void)
{
    system_init();

    __enable_irq();

    for (;;)
    {
        button_event_t button_event_id = get_button_event();

        switch (button_event_id) {
            case BUTTON_S1_PRESSED:
                save_button_event(BUTTON_S1_PRESSED);
                uart_send_line("button S1 pressed");
                break;
            case BUTTON_S2_PRESSED:
                save_button_event(BUTTON_S2_PRESSED);
                uart_send_line("button S2 pressed");
                break;
            case BUTTON_S3_PRESSED:
                save_button_event(BUTTON_S3_PRESSED);
                uart_send_line("button S3 pressed");
                break;
            case BUTTON_S1_RELEASED:
                save_button_event(BUTTON_S1_RELEASED);
                uart_send_line("button S1 released");
                break;
            case BUTTON_S2_RELEASED:
                save_button_event(BUTTON_S2_RELEASED);
                uart_send_line("button S2 released");
                break;
            case BUTTON_S3_RELEASED:
                save_button_event(BUTTON_S3_RELEASED);
                uart_send_line("button S3 released");
                break;
            default:
                break;
        }
        
        can_receive_message();
    }
}