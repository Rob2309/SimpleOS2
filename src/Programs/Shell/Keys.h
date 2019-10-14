#pragma once

#include "types.h"

enum Key : uint16 {
    KEY_0 = '0',
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    KEY_A = 'a',
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,

    KEY_QUOTE = '\'',
    
    KEY_BACKSPACE = '\b',
    KEY_TAB = '\t',
    KEY_ENTER = '\n',

    KEY_PLUS = '+',
    KEY_MINUS = '-',
    KEY_COMMA = ',',
    KEY_PERIOD = '.',
    KEY_SPACE = ' ',

    KEY_CTRL_LEFT = 0x0100,
    KEY_CTRL_RIGHT = 0x0101,
    KEY_SHIFT_LEFT = 0x0102,
    KEY_SHIFT_RIGHT = 0x0103,

    KEY_UP = 0x0200,
    KEY_DOWN = 0x0201,
    KEY_LEFT = 0x0202,
    KEY_RIGHT = 0x0203,



    KEY_RELEASED = 0x8000,
};

