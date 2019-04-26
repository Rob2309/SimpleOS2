#include "Syscall.h"

extern "C" void main()
{
    Syscall::Print("Hello world from Test2!\n");

    Syscall::Exit(0);
}