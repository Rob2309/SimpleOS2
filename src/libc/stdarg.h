#pragma once

#define va_list __builtin_va_list

#define va_start(list, startArg) __builtin_va_start(list, startArg)
#define va_arg(list, type) __builtin_va_arg(list, type)
#define va_end(list) __builtin_va_end(list)
#define va_copy(listA, listB) __builting_va_copy(listA, listB)