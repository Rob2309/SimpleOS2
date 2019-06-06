#pragma once

#include "types.h"

/**
 * Very rudimentary printf-like function
 * %i: Decimal 64-Bit Integer
 * %X: Hexadecimal 64-Bit Integer
 * %x: Hexadecimal 64-Bit Integer, zero extended
 * %s: String
 **/ 
void kprintf(const char* format, ...);

/**
 * Set the color in which text is printed by printf
 **/
void kprintf_setcolor(uint32 color);
/**
 * Set the color in which text is printed by printf
 **/
void kprintf_setcolor(uint8 r, uint8 g, uint8 b);