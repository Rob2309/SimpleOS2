#include <KernelHeader.h>

#include "terminal/terminal.h"
#include "terminal/conio.h"
#include "memory/MemoryManager.h"
#include "arch/GDT.h"
#include "interrupts/IDT.h"
#include "arch/APIC.h"

#include "scheduler/Scheduler.h"

#include "scheduler/ELF.h"

#include "fs/Ramdisk.h"

#include "memory/KernelHeap.h"

#include "fs/VFS.h"
#include "devices/RamDevice.h"
#include "devices/ZeroDevice.h"

#include "syscalls/SyscallHandler.h"

#include "memory/memutil.h"

#include "scheduler/Process.h"

#include "arch/CPU.h"

static void SetupTestProcess(uint8* loadBase)
{
    uint64 file = VFS::OpenNode("/initrd/test.elf");
    if(file == 0) {
        printf("Failed to find test.elf\n");
        return;
    }

    uint64 fileSize = VFS::GetSize(file);
    uint8* fileBuffer = new uint8[fileSize];
    VFS::ReadNode(file, 0, fileBuffer, fileSize);
    VFS::CloseNode(file);

    if(!RunELF(fileBuffer)) {
        printf("Failed to setup process\n");
        return;
    }

    delete[] fileBuffer;
}

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    Terminal::Init(info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth, info->screenColorsInverted);
    Terminal::Clear();

    printf("Kernel at 0x%x\n", info->kernelImage.buffer);
    
    MemoryManager::Init(info);

    GDT::Init(info);
    IDT::Init();
    SyscallHandler::Init();

    VFS::Init();

    if(!VFS::CreateFolder("/", "dev"))
        printf("Failed to create /dev folder\n");
    if(!VFS::CreateFolder("/", "initrd"))
        printf("Failed to create /initrd folder\n");

    new RamDevice("ram0", info->ramdiskImage.buffer, info->ramdiskImage.numPages * 4096);
    RamdiskFS* ramfs = new RamdiskFS("/dev/ram0");
    VFS::Mount("/initrd", ramfs);

    new ZeroDevice("zero");

    APIC::Init();

    VFS::CreateFile("/", "test.txt");

    SetupTestProcess((uint8*)0x16000);
    //SetupTestProcess((uint8*)0x16000);
    APIC::StartTimer(10);
    Scheduler::Start();

    // should never be reached
    while(true) {
        __asm__ __volatile__ (
            "hlt"
        );
    }

}