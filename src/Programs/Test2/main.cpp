#include "Syscall.h"

const char msg[] = "Hello World from Test2\n";

extern "C" void main()
{
    Syscall::Exit(0);
}