#include "Syscall.h"
#include "Mutex.h"

static uint64 g_Descs[2];

static char g_Thread1Stack[3 * 4096];
static void Thread1()
{
    static char s_Buffer[4096];

    Syscall::Print("Reading pipe...\n");
    Syscall::Read(g_Descs[1], 0, s_Buffer, sizeof(s_Buffer));

    Syscall::Print(s_Buffer);
    Syscall::Exit(0);
}

extern "C" void main()
{
    Syscall::Pipe(g_Descs);

    Syscall::CreateThread((uint64)&Thread1, (uint64)g_Thread1Stack + sizeof(g_Thread1Stack));
    Syscall::Wait(1000);

    const char msg[] = "Hello World\n";
    Syscall::Write(g_Descs[0], 0, (void*)msg, sizeof(msg));
    
    Syscall::Exit(0);
}