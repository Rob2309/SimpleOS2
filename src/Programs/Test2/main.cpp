#include "Syscall.h"

#include "stdio.h"
#include "assert.h"

extern "C" void main()
{
    puts("Hello World from Test program\n");
    puts("This is another message\n");

    FILE* file = fopen("/log.txt", "w");
    if(file == nullptr)
        puts("Failed to open /log.txt for writing\n");

    Syscall::Exit(0);
}