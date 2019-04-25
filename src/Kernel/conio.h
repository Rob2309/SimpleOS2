#pragma once

#include "types.h"

/**
 * Very rudimentary printf-like function
 * %i: Decimal 64-Bit Integer
 * %X: Hexadecimal 64-Bit Integer
 * %x: Hexadecimal 64-Bit Integer, zero extended
 * %s: String
 **/ 
void printf(const char* format, ...);

/**
 * Set the color in which text is printed by printf
 **/
void SetTerminalColor(uint32 color);
/**
 * Set the color in which text is printed by printf
 **/
void SetTerminalColor(uint8 r, uint8 g, uint8 b);