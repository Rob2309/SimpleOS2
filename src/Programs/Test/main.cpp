#include "Syscall.h"
#include "Mutex.h"

static char g_Buffer[4096];

extern "C" void main()
{
    Syscall::Print("Hello World from Test Program\n");

    Syscall::Wait(1000);

    Syscall::Print("Test Program finished...\n");
    Syscall::Exit(0);
}