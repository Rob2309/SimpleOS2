#include "simpleos_process.h"

#include "stdio.h"
#include "assert.h"

extern "C" void main()
{
    puts("Hello World from Test program\n");
    puts("This is another message\n");

    thread_exit(0);
}