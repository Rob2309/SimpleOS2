#include <KernelHeader.h>

#include "terminal.h"
#include "conio.h"
#include "MemoryManager.h"
#include "GDT.h"
#include "IDT.h"
#include "APIC.h"

#include "Scheduler.h"

#include "ELF.h"

#include "Syscall.h"

#include "Ramdisk.h"

#include "KernelHeap.h"

#include "VFS.h"
#include "RamDevice.h"
#include "ZeroDevice.h"

#include "SyscallHandler.h"

static void SetupTestProcess(uint8* loadBase)
{
    uint64 file = VFS::GetFileNode("/initrd/test.elf");
    if(file == 0) {
        printf("Failed to find test.elf\n");
        return;
    }
    if(!VFS::AddFileUserRead(file)) {
        printf("Failed to open test.elf\n");
        return;
    }

    uint64 fileSize = VFS::GetFileSize(file);
    uint8* fileBuffer = new uint8[fileSize];
    VFS::ReadFile(file, 0, fileBuffer, fileSize);
    VFS::RemoveFileUserRead(file);

    uint64 pages = (GetELFSize(fileBuffer) + 4095) / 4096;
    uint64 pml4Entry = MemoryManager::CreateProcessMap();
    MemoryManager::SwitchProcessMap(pml4Entry);

    uint8* processBuffer = (uint8*)MemoryManager::AllocatePages(pages);
    for(uint64 i = 0; i < pages; i++) {
        MemoryManager::MapProcessPage(pml4Entry, processBuffer + i * 4096, loadBase + i * 4096);
    }

    Elf64Addr entryPoint;
    if(!PrepareELF(fileBuffer, loadBase, &entryPoint)) {
        printf("Failed to setup process\n");
        return;
    }

    uint8* stack = (uint8*)MemoryManager::AllocatePages(10);
    for(int i = 0; i < 10; i++)
        MemoryManager::MapProcessPage(pml4Entry, stack + i * 4096, (void*)(0x1000 + i * 4096));

    uint8* kernelStack = (uint8*)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages(3)) + Scheduler::KernelStackSize;

    Scheduler::RegisterProcess(pml4Entry, 0x1000 + 10 * 4096, entryPoint, true, (uint64)kernelStack);

    delete[] fileBuffer;
}

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    Terminal::Init(info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth, info->screenColorsInverted);
    Terminal::Clear();

    SetTerminalColor(180, 180, 180);
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