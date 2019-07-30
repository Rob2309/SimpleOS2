#include "assert.h"

#include "stdio.h"
#include "stdlib.h"

#include "syscall.h"

void __do_assert(int cond, const char* msg) {
    if(!cond) {
        fputs(msg, stdout);
        abort();
    }
}