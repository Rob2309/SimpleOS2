#include "Syscall.h"

extern "C" void main()
{
    const char msg[] = "Hello World from Test2\n";

    Syscall::Write(1, 0, (void*)msg, sizeof(msg));
    Syscall::Exit(0);
}