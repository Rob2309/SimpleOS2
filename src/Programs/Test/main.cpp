#include "Syscall.h"
#include "Mutex.h"

static uint64 g_Descs[2];

extern "C" void main()
{
    Syscall::Pipe(g_Descs);

    if(Syscall::Fork()) {
        Syscall::Print("Parent process alive\n");

        static char s_Buffer[128] = { 0 };

        while(true) {
            Syscall::Read(2, 0, s_Buffer, sizeof(s_Buffer));
            Syscall::Print(s_Buffer);
        }
    } else {
        Syscall::Print("Executing test2...\n");
        Syscall::Exec("/initrd/test2.elf");
        Syscall::Print("Failed to exec!\n");
        Syscall::Exit(1);
    }

    Syscall::Exit(0);
}