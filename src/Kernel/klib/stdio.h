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

template<typename... Args>
inline void kprintf_colored(uint8 r, uint8 g, uint8 b, const char* format, const Args&... args) {
    kprintf_setcolor(r, g, b);
    kprintf(format, args...);
    kprintf_setcolor(255, 255, 255);
}

#define klog_info(context, format, ...)     kprintf_colored(170, 170, 170, "[INFO][%s] " format "\n", context, ##__VA_ARGS__)
#define klog_warning(context, format, ...)  kprintf_colored(40, 255, 255, "[WARNING][%s] " format "\n", context, ##__VA_ARGS__)
#define klog_error(context, format, ...)    kprintf_colored(255, 40, 40, "[ERROR][%s] " format "\n", context, ##__VA_ARGS__)
#define klog_fatal(context, format, ...)    kprintf_colored(255, 40, 40, "[FATAL][%s] " format "\n", context, ##__VA_ARGS__)