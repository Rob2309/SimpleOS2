#include "Syscall.h"

extern "C" void main()
{
    while(true) {
        Syscall::Print("Program 1 running\n");
        Syscall::Wait(1000);
    }
}