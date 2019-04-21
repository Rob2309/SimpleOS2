#include "Syscall.h"
#include "Mutex.h"

static uint64 g_Descs[2];

extern "C" void main()
{
    Syscall::Pipe(g_Descs);

    if(Syscall::Fork() == 0) {
        Syscall::Print("Child process alive...\n");

        static char s_Buffer[4096];
        uint64 count = Syscall::Read(g_Descs[1], 0, s_Buffer, sizeof(s_Buffer));
        if(count == 0)
            Syscall::Print("Failed to read pipe\n");
        Syscall::Print(s_Buffer);
    } else {
        Syscall::Print("Parent process alive...\n");

        const char msg[] = "Hello World\n";
        uint64 count = Syscall::Write(g_Descs[0], 0, (void*)msg, sizeof(msg));
        if(count == 0)
            Syscall::Print("Failed to write pipe\n");
    }

    Syscall::Exit(0);
}