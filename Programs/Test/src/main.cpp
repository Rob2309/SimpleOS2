#include "Syscall.h"

extern "C" void main()
{
    Syscall::Print("Parent starting\n");

    if(Syscall::Fork() == 0) {
        Syscall::Print("Child process\n");
        Syscall::Exit(0);
    } else {
        Syscall::Print("Parent process\n");
    }
    
    while(true);
}