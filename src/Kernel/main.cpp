#include <KernelHeader.h>

#include "terminal/terminal.h"
#include "klib/stdio.h"
#include "memory/MemoryManager.h"
#include "arch/GDT.h"
#include "interrupts/IDT.h"
#include "arch/APIC.h"

#include "scheduler/Scheduler.h"

#include "scheduler/ELF.h"

#include "memory/KernelHeap.h"

#include "fs/VFS.h"
#include "fs/ext2/ext2.h"
#include "devices/PseudoDeviceDriver.h"
#include "devices/RamDeviceDriver.h"

#include "syscalls/SyscallHandler.h"

#include "scheduler/Process.h"

#include "arch/CPU.h"

#include "fs/TestFS.h"

static void SetupTestProcess(uint8* loadBase)
{
    uint64 file = VFS::Open("/initrd/Test.elf");
    if(file == 0) {
        klog_error("Init", "Failed to find Test.elf");
        return;
    }

    VFS::NodeStats stats;
    VFS::Stat(file, &stats);
    uint8* fileBuffer = new uint8[stats.size];
    VFS::Read(file, 0, fileBuffer, stats.size);
    VFS::Close(file);

    if(!RunELF(fileBuffer)) {
        klog_error("Init", "Failed to setup process");
        return;
    }

    delete[] fileBuffer;
}

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    Terminal::Init(info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth, info->screenColorsInverted);
    Terminal::Clear();

    klog_info("Kernel", "Kernel at 0x%x", info->kernelImage.buffer);
    
    MemoryManager::Init(info);

    GDT::Init(info);
    IDT::Init();
    SyscallHandler::Init();

    Ext2::Init();

    VFS::Init(new TestFS());
    VFS::CreateFolder("/dev");

    RamDeviceDriver* ramDriver = new RamDeviceDriver();
    uint64 initrdSubID = ramDriver->AddDevice((char*)info->ramdiskImage.buffer, 512, info->ramdiskImage.numPages * 8);
    VFS::CreateDeviceFile("/dev/ram0", ramDriver->GetDriverID(), initrdSubID);

    PseudoDeviceDriver* pseudoDriver = new PseudoDeviceDriver();
    VFS::CreateDeviceFile("/dev/zero", pseudoDriver->GetDriverID(), PseudoDeviceDriver::DeviceZero);

    VFS::CreateFolder("/initrd");
    VFS::Mount("/initrd", "ext2", "/dev/ram0");

    APIC::Init();

    SetupTestProcess((uint8*)0x16000);
    APIC::StartTimer(10);
    Scheduler::Start();

    // should never be reached
    while(true) {
        __asm__ __volatile__ (
            "hlt"
        );
    }

}