#include "Syscall.h"

static char g_Buffer[4096];

extern "C" void main()
{
    Syscall::Print("Parent starting\n");

    //if(Syscall::Fork() == 0) {
        Syscall::Print("Child process\n");
    
        for(int i = 0; i < 5; i++) {
            Syscall::Wait(1000);
            Syscall::Print("Child process alive\n");
        }

        Syscall::Exit(0);
    //} else {
        /*Syscall::Print("Parent process\n");

        for(int i = 0; i < 5; i++) {
            Syscall::Wait(1000);
            Syscall::Print("Parent process alive\n");
        }

        Syscall::Exit(0);*/
    //}
    
    Syscall::Exit(0);
}