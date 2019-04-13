#include "Syscall.h"

static char g_Thread1Stack[4096];

unsigned int g_Test = 17;

void Thread1()
{
    Syscall::Wait(500);
    g_Test = 23;
    Syscall::Exit(0);
}

extern "C" void main()
{
    Syscall::Print("Creating thread...\n");
    Syscall::CreateThread((uint64)&Thread1, (uint64)&g_Thread1Stack + 4096);
    Syscall::Print("Thread created...\n");

    while(true)
    {
        if(g_Test != 17) {
            Syscall::Print("g_Test changed!\n");
        } else {
            Syscall::Print("g_Test still old\n");
        }
        Syscall::Wait(250);
    }
    
    Syscall::Exit(0);
}