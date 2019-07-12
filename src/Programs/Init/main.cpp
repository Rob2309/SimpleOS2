#include "Syscall.h"

#define CHECK_ERROR(func, msg) \
    if(func < 0) { Syscall::Print(msg "\n"); Syscall::Exit(1); }

extern "C" void main()
{
    CHECK_ERROR(Syscall::CreateFolder("/dev"), "Failed to create /dev folder");
    CHECK_ERROR(Syscall::CreateDeviceFile("/dev/zero", 0, 0), "Failed to create /dev/zero");
    CHECK_ERROR(Syscall::CreateDeviceFile("/dev/ram0", 1, 0), "Failed to create /dev/ram0");

    Syscall::Print("Executing test program\n");
    Syscall::Exec("/boot/Test2.elf");

    Syscall::Print("Exec failed!");
    Syscall::Exit(1);
}