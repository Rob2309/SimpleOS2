#include "Syscall.h"

const char msg[] = "Hello World from Test2\n";

extern "C" void main()
{
    Syscall::Write(1, 0, msg, sizeof(msg));
    Syscall::Exit(0);
}