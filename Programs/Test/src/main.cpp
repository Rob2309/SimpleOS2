#include "Syscall.h"

extern "C" void main()
{
    uint64 pid = Syscall::GetPID();
    if(pid == 2)
        Syscall::Wait(500);

    while(true) {
        if(pid == 1) {
            Syscall::Print("Process 1 running\n");
            Syscall::Wait(1000);
        } else {
            Syscall::Print("Process 2 running\n");
            Syscall::Wait(1000);
        }
    }
}