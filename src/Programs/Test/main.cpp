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
    Syscall::MoveThreadToCore(1);

    int64 error = Syscall::CreateFile("/dev/test");
    if(error < 0)
        Syscall::Print("Failed to create /dev/test (expected)\n");

    error = Syscall::CreateFile("/user/testFile");
    if(error < 0)
        Syscall::Print("Failed to create /user/testFile\n");

    int64 fd = Syscall::Open("/dev/ram0", 1);
    if(fd < 0)
        Syscall::Print("Failed to open /dev/ram0 for reading\n");

    int64 fd2 = Syscall::Open("/dev/ram0", 3);
    if(fd2 < 0)
        Syscall::Print("Failed to open /dev/ram0 for writing (expected)\n");

    int64 fd3 = Syscall::Open("/user/testFile", 3);
    if(fd3 < 0)
        Syscall::Print("Failed to open /user/testFile\n");

    char buffer[255];
    
    error = Syscall::Write(fd, buffer, sizeof(buffer));
    if(error < 0)
        Syscall::Print("Failed to write to readonly fd (expected)\n");

    error = Syscall::Write(fd3, buffer, sizeof(buffer));
    if(error < 0)
        Syscall::Print("Failed to write to writable fd\n");

    Syscall::Exit(0);
}