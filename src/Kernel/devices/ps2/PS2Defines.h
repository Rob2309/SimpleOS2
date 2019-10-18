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

    static uint16 g_ScanMap[256] =
	{
		0, 0,
		'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
		0, '\'', '\b', '\t',
		'q', 'w', 'e', 'r', 't', 'z', 'u', 'i', 'o', 'p',
		'u', '+', '\n', 0,
		'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
		'o', 'a', 0, 0, 0,
		'y', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-',
		0, 0, 0,
		' ',
	};
	/*static char g_ScanMapShift[256] =
	{
		0, 0,
		'!', '"', 0, '$', '%', '&', '/', '(', ')', '=',
		'?', '`', '\b', '\t',
		'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I', 'O', 'P',
		'U', '*', '\n', 0,
		'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
		'O', 'A', 0, 0, 0,
		'Y', 'X', 'C', 'V', 'B', 'N', 'M', ';', ':', '_',
		0, 0, 0,
		' ',
	};*/

}