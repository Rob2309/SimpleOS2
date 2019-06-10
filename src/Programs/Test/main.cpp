#include "Syscall.h"
#include "Mutex.h"

static char g_Buffer[4096];

extern "C" void main()
{
    uint64 readDesc, writeDesc;
    Syscall::CreatePipe(&readDesc, &writeDesc);

    if(Syscall::Fork()) {
        Syscall::Wait(2000);

        const char msg[] = "Hello World\n";
        Syscall::Write(writeDesc, msg, sizeof(msg));
        Syscall::Print("Written to pipe\n");
    } else {
        char buffer[128];
        Syscall::Read(readDesc, buffer, sizeof(buffer));
        Syscall::Print(buffer);
    }

    Syscall::Exit(0);
}