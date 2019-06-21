#include "Syscall.h"
#include "Mutex.h"

extern "C" void main()
{
    //if(Syscall::Fork()) {
        Syscall::Print("Parent...\n");

        for(int i = 0; i < 5; i++) {
            Syscall::Wait(2000);
            Syscall::Print("Parent alive...\n");
        }

        Syscall::Print("Lets allocate pages in the kernel memory space!\n");
        Syscall::AllocPages((void*)0xFFFFFF8000123000, 10);
    /*} else {
        Syscall::Print("Child...\n");

        Syscall::Exec("/initrd/Test2.elf");

        Syscall::Print("Failed to exec...\n");
    }*/

    Syscall::Exit(0);
}