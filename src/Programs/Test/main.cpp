#include "Syscall.h"

static char g_Buffer[4096];

extern "C" void main()
{
    Syscall::Print("Parent starting\n");

    /*if(Syscall::Fork() == 0) {
        Syscall::Print("Child process\n");

        for(int i = 0; i < 5; i++) {
            Syscall::Wait(1000);
            Syscall::Print("Child process alive\n");
        }

        Syscall::Exit(0);
    } else {
        Syscall::Print("Parent process\n");

        uint64 f = Syscall::Open("/dev/zero");
        if(f == 0) {
            Syscall::Print("Failed to open /dev/zero\n");
            Syscall::Exit(1);
        }
        
        for(int i = 0; i < sizeof(g_Buffer); i++)
            g_Buffer[i] = i;
        Syscall::Read(f, 0, g_Buffer, sizeof(g_Buffer));
        for(int i = 0; i < sizeof(g_Buffer); i++)
            if(g_Buffer[i] != 0) {
                Syscall::Print("/dev/zero not working\n");
                Syscall::Exit(1);
            }
        Syscall::Close(f);
    }*/
    
    while(true);
}