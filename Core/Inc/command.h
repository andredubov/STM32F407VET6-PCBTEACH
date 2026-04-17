#ifndef COMMAND_H
#define COMMAND_H

typedef enum {
    CMD_NONE = 0,
    TURN_ALL_LEDS_OFF = '0',
    TURN_LED_1_ON = '1',
    TURN_LED_2_ON = '2',
    TURN_LED_3_ON = '3',
    ERASE_EEPROM = '4'
} command_id_t;

#endif // COMMAND_H