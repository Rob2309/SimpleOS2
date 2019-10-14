#pragma once

#include "types.h"

/**
 * Very rudimentary printf-like function
 * Standard integer format specifiers supported
 * Additional format specifiers:
 *      %C: change color, given as three 8 bit integers
 **/ 
void kprintf(const char* format, ...);
void kvprintf(const char* format, __builtin_va_list arg);
void kfprintf(uint64 fd, const char* format, ...);
void kfvprintf(uint64 fd, const char* format, __builtin_va_list arg);

void kprintf_isr(const char* format, ...);
void kvprintf_isr(const char* format, __builtin_va_list arg);

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

#define klog_info_isr(context, format, ...)     kprintf_isr("%C[INFO ][%S] " format "\n", 170, 170, 170, context, 10, ##__VA_ARGS__)
#define klog_warning_isr(context, format, ...)  kprintf_isr("%C[WARN ][%S] " format "\n", 255, 255, 40, context, 10, ##__VA_ARGS__)
#define klog_error_isr(context, format, ...)    kprintf_isr("%C[ERROR][%S] " format "\n", 255, 40, 40, context, 10, ##__VA_ARGS__)
#define klog_fatal_isr(context, format, ...)    kprintf_isr("%C[FATAL][%S] " format "\n", 255, 40, 40, context, 10, ##__VA_ARGS__)