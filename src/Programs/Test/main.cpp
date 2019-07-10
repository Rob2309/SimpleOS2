#include "Syscall.h"

static char g_Stack1[4096];
static char g_Stack2[4096];

static void Thread1() {
    Syscall::MoveThreadToCore(1);
    while(true) {
        Syscall::Print("Thread 1 alive...\n");
        Syscall::Wait(1000);
    }
}

static void Thread2() {
    Syscall::MoveThreadToCore(2);
    while(true) {
        Syscall::Print("Thread 2 alive...\n");
        Syscall::Wait(1000);
    }
}

extern "C" void main()
{
    /*if(Syscall::Fork()) {
        Syscall::CreateThread((uint64)&Thread1, (uint64)&g_Stack1[4096]);
        Syscall::CreateThread((uint64)&Thread2, (uint64)&g_Stack2[4096]);

        for(int i = 0; i < 5; i++) {
            Syscall::Print("Thread 0 alive...\n");
            Syscall::Wait(1000);
        }

        Syscall::Print("Lets allocate pages in the kernel memory space!\n");
        Syscall::AllocPages((void*)0xFFFFFF8000123000, 10);
    } else {
        while(true) {
            Syscall::Print("Alive...\n");
            Syscall::Wait(1000);
        }
    }*/

    if(!Syscall::Open("/initrd/Test.elf"))
        Syscall::Print("Permission denied\n");

    /*if(Syscall::Fork()) {
        Syscall::Print("Parent...\n");

        for(int i = 0; i < 5; i++) {
            Syscall::Wait(2000);
            Syscall::Print("Parent alive...\n");
        }

        Syscall::Print("Lets allocate pages in the kernel memory space!\n");
        Syscall::AllocPages((void*)0xFFFFFF8000123000, 10);
    } else {
        Syscall::Print("Child...\n");

        Syscall::Exec("/initrd/Test2.elf");

        Syscall::Print("Failed to exec...\n");
    }*/



    Syscall::Exit(0);
}