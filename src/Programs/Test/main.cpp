#include "Syscall.h"
#include "Mutex.h"

static uint64 g_Descs[2];

extern "C" void main()
{
    Syscall::Print("Hello World\n");

    Syscall::Exit(0);
}