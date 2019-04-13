#include "Syscall.h"
#include "Mutex.h"

static char g_Thread1Stack[4096];

Mutex g_Lock;

void Thread1()
{
    g_Lock.SpinLock();

    for(int i = 0; i < 5; i++) {
        Syscall::Print("Sub thread has lock\n");
        Syscall::Wait(500);
    }

    g_Lock.Unlock();
    Syscall::Exit(0);
}

extern "C" void main()
{
    Syscall::CreateThread((uint64)&Thread1, (uint64)&g_Thread1Stack + 4096);
    
    Syscall::Wait(500);

    Syscall::Print("Main thread waiting for lock\n");
    Syscall::WaitForLock(&g_Lock);
    Syscall::Print("Main thread has lock\n");

    Syscall::Exit(0);
}