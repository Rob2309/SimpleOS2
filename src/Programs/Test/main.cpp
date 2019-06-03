#include "Syscall.h"
#include "Mutex.h"

static char g_Buffer[4096];

extern "C" void main()
{
    if(!Syscall::CreateFolder("/test"))
        Syscall::Print("Failed to create /test\n");

    if(!Syscall::CreateDeviceFile("/test/zero", 1, 0))
        Syscall::Print("Failed to create /test/zero\n");

    uint64 desc = Syscall::Open("/test/zero");
    if(!Syscall::Delete("/test/zero"))
        Syscall::Print("Failed to delete /test/zero\n");

    for(int i = 0; i < sizeof(g_Buffer); i++)
        g_Buffer[i] = i;
    Syscall::Read(desc, g_Buffer, sizeof(g_Buffer));
    Syscall::Close(desc);

    for(int i = 0; i < sizeof(g_Buffer); i++)
        if(g_Buffer[i] != 0)
            Syscall::Print("/test/zero not working!\n");

    Syscall::Exit(0);
}