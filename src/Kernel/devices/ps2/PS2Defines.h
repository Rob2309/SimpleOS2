#pragma once

#include "types.h"

namespace PS2 {

    #define REG_DATA 0x60
    #define REG_STATUS 0x64
    #define REG_COMMAND 0x64

    #define STATUS_OUTPUT_BUFFER_FULL 0x1
    #define STATUS_INPUT_BUFFER_FULL 0x2

    #define CMD_READ_CONFIG 0x20
    #define CMD_WRITE_CONFIG 0x60

    #define CONFIG_PORTA_INT 0x1
    #define CONFIG_PORTB_INT 0x2

}