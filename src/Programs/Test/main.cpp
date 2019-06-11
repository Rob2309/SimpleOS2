#include "Syscall.h"
#include "Mutex.h"

extern "C" void main()
{
    if(Syscall::Fork()) {
        Syscall::Print("Parent...\n");
    } else {
        Syscall::Print("Child...\n");

        Syscall::Exec("/initrd/Test2.elf");

        Syscall::Print("Failed to exec...\n");
    }

    Syscall::Exit(0);
}