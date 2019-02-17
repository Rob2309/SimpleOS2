#include "Syscall.h"

extern "C" void main()
{
    uint64 pid = Syscall::GetPID();
    if(pid == 2)
        Syscall::Wait(500);

    int i = 0;
    while(i < 5) {
        if(pid == 1) {
            Syscall::Print("Process 1 running\n");
            Syscall::Wait(1000);
            i++;
        } else {
            Syscall::Print("Process 2 running\n");
            Syscall::Wait(1000);
        }
    }

    Syscall::Exit(0);
}