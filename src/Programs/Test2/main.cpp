#include "Syscall.h"

#include "stdio.h"

extern "C" void main()
{
    puts("Hello World from Test program\n");
    puts("This is another message\n");

    FILE* file = fopen("/boot/Test2.elf", "r");
    if(file == nullptr) 
        puts("Failed to open /boot/Test2.elf\n");

    char buffer[5] = { 0 };
    fread(buffer, 1, 4, file);
    fclose(file);

    puts(buffer);

    Syscall::Exit(0);
}