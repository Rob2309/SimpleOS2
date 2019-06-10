#include "Syscall.h"

const char msg[] = "Hello World from Test2\n";

extern "C" void main()
{
    Syscall::Print("Hello World 2\n");
    Syscall::Exit(0);
}