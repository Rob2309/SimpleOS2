#include "simpleos_raw.h"

#include "syscall.h"

void print(const char* msg) {
    syscall_invoke(syscall_print, (uint64)msg);
}