#include "Syscall.h"

extern "C" void main()
{
    Syscall::Print("Test program starting...\n");

    uint64 readDesc, writeDesc;
    Syscall::CreatePipe(&readDesc, &writeDesc);

    Syscall::Print("Created communication pipe\n");

    int64 stdout = Syscall::Open("/initrd/Test.elf", 1);
    if(stdout < 0)
        Syscall::Print("Failed to open /initrd/Test.elf\n");

    Syscall::Print("Forking...\n");
    
    if(Syscall::Fork()) {
        // parent
        Syscall::Print("Replacing FD\n");
        if(Syscall::ReplaceFD(stdout, writeDesc) < 0)
            Syscall::Print("Failed to replace fd\n");

        Syscall::Print("Writing to pipe\n");
        const char msg[] = "Hello World\n";
        if(Syscall::Write(stdout, msg, sizeof(msg)) < 0) 
            Syscall::Print("Failed to write to pipe\n");

        Syscall::Print("Written to pipe\n");
    } else {
        // child
        Syscall::Print("Reading from pipe\n");
        char buffer[256];
        if(Syscall::Read(readDesc, buffer, sizeof(buffer)) < 0)
            Syscall::Print("Failed to read from pipe\n");
        Syscall::Print(buffer);
    }

    Syscall::Exit(0);
}