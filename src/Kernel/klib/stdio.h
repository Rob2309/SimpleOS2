#pragma once

#include "types.h"

/**
 * Very rudimentary printf-like function
 * %i: Decimal 64-Bit Integer
 * %X: Hexadecimal 64-Bit Integer
 * %x: Hexadecimal 64-Bit Integer, zero extended
 * %s: String
 * %S: String, centered, padding size given in uint64 after the string
 * %c: change color, given as one 32 bit integer
 * %C: change color, given as three 8 bit integers
 **/ 
void kprintf(const char* format, ...);

/**
 * Set the color in which text is printed by printf by default
 **/
void kprintf_setcolor(uint32 color);
/**
 * Set the color in which text is printed by printf by default
 **/
void kprintf_setcolor(uint8 r, uint8 g, uint8 b);

#define klog_info(context, format, ...)     kprintf("%C[INFO ][%S] " format "\n", 170, 170, 170, context, 10, ##__VA_ARGS__)
#define klog_warning(context, format, ...)  kprintf("%C[WARN ][%S] " format "\n", 255, 255, 40, context, 10, ##__VA_ARGS__)
#define klog_error(context, format, ...)    kprintf("%C[ERROR][%S] " format "\n", 255, 40, 40, context, 10, ##__VA_ARGS__)
#define klog_fatal(context, format, ...)    kprintf("%C[FATAL][%S] " format "\n", 255, 40, 40, context, 10, ##__VA_ARGS__)