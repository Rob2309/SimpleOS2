#pragma once

#undef ACPI_DEBUGGER
#undef ACPI_DISASSEMBLER

#define ACPI_SPINLOCK void*
#define ACPI_SEMAPHORE void*
#define ACPI_MUTEX_TYPE ACPI_BINARY_SEMAPHORE

#define ACPI_CPU_FLAGS UINT64

#define ACPI_CACHE_T ACPI_MEMORY_LIST
#define ACPI_USE_LOCAL_CACHE

#define ACPI_USE_DO_WHILE_0

#define ACPI_MACHINE_WIDTH 64

#define va_list __builtin_va_list
#define va_arg __builtin_va_arg
#define va_start __builtin_va_start
#define va_end __builtin_va_end