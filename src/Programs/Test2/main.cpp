#include "Syscall.h"

extern "C" void main()
{
    Syscall::Print("Hello world from test program\n");

    Syscall::Exit(0);
}