#ifndef TASK_H
#define TASK_H

#include <stdbool.h>
#include "led.h"
#include "button.h"

void save_button_event(button_event_t button_id);
void send_button_state(void);
void can_receive_message(void);

#endif // TASK_H