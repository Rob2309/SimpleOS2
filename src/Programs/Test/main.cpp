#include "Syscall.h"
#include "Mutex.h"

static uint64 g_Descs[2];

extern "C" void main()
{
    if(!Syscall::CreateFolder("/test"))
        Syscall::Print("Failed to create /test\n");

    if(!Syscall::CreateFile("/test/file1"))
        Syscall::Print("Failed to create /test/file1\n");

    uint64 desc = Syscall::Open("/test/file1");

    Syscall::Exit(0);
}