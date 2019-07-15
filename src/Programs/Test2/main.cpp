#include "Syscall.h"

#include "stdio.h"
#include "assert.h"

extern "C" void main()
{
    puts("Hello World from Test program\n");
    puts("This is another message\n");

    Syscall::Exit(0);
}