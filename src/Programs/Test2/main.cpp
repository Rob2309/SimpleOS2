#include "Syscall.h"

const char msg[] = "Hello World from Test2\n";

extern "C" void main()
{
    Syscall::Print("Hello World 2\n");

    Syscall::Print("Creating /test\n");
    if(!Syscall::CreateFolder("/test"))
        Syscall::Print("Failed to create /test\n");

    Syscall::Print("Mounting TestFS onto /test\n");
    if(!Syscall::Mount("/test", "test"))
        Syscall::Print("Failed to mount TestFS\n");

    Syscall::Print("Creating /test/file1\n");
    if(!Syscall::CreateFile("/test/file1"))
        Syscall::Print("Failed to create /test/file1\n");

    const char msg[] = "Hello world from file1\n";
    uint64 file1 = Syscall::Open("/test/file1");
    Syscall::Write(file1, msg, sizeof(msg));
    Syscall::Close(file1);

    char buffer[128];
    uint64 file2 = Syscall::Open("/test/file1");
    Syscall::Read(file2, buffer, sizeof(buffer));
    Syscall::Close(file2);

    Syscall::Print(buffer);

    Syscall::Exit(0);
}