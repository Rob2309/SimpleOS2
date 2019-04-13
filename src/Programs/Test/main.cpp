#include "Syscall.h"
#include "Mutex.h"

static char g_Thread1Stack[4096];

Mutex g_Lock;

void Thread1()
{
    Syscall::WaitForLock(&g_Lock);
    Syscall::Print("Sub Thread has lock\n");
    g_Lock.Unlock();

    Syscall::Exit(0);
}

extern "C" void main()
{
    Syscall::CreateThread((uint64)&Thread1, (uint64)&g_Thread1Stack + 4096);

    g_Lock.SpinLock();
    Syscall::Print("Main Thread has lock\n");
    Syscall::Wait(500);
    g_Lock.Unlock();
    
    Syscall::Exit(0);
}