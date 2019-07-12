#include "Syscall.h"

#define CHECK_ERROR(func, msg) \
    if(func < 0) { Syscall::Print(msg "\n"); Syscall::Exit(1); }

extern "C" void main()
{
    CHECK_ERROR(Syscall::CreateFolder("/dev"), "Failed to create /dev folder");
    CHECK_ERROR(Syscall::CreateDeviceFile("/dev/zero", 0, 0), "Failed to create /dev/zero");
    CHECK_ERROR(Syscall::CreateDeviceFile("/dev/ram0", 1, 0), "Failed to create /dev/ram0");

    int64 stdinRead, stdinWrite, stdoutRead, stdoutWrite;
    Syscall::CreatePipe(&stdinRead, &stdinWrite);
    Syscall::CreatePipe(&stdoutRead, &stdoutWrite);

    if(Syscall::Fork()) {
        Syscall::Close(stdinRead);
        Syscall::Close(stdoutWrite);

        while(true) {
            char buffer[128];
            int64 len = Syscall::Read(stdoutRead, buffer, sizeof(buffer)-1);
            buffer[len] = 0;
            Syscall::Print(buffer);
        }
    } else {
        Syscall::ReplaceFD(1, stdoutWrite);

        Syscall::Exec("/boot/Test2.elf");
        Syscall::Exit(1);
    }
}