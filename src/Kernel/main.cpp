#include "KernelHeader.h"

#include "terminal/terminal.h"
#include "klib/stdio.h"
#include "arch/GDT.h"
#include "interrupts/IDT.h"
#include "arch/APIC.h"
#include "memory/MemoryManager.h"
#include "multicore/SMP.h"

#include "fs/VFS.h"
#include "fs/TestFS.h"
#include "devices/RamDeviceDriver.h"
#include "fs/ext2/ext2.h"

#include "scheduler/Scheduler.h"
#include "scheduler/ELF.h"

static void SetupTestProcess() {
    uint64 file = VFS::Open("/initrd/Test.elf");
    if(file == 0) {
        klog_error("Test", "Failed to open /initrd/Test.elf");
        return;
    }
    
    VFS::NodeStats stats;
    VFS::Stat(file, &stats);
    
    uint8* buffer = new uint8[stats.size];
    VFS::Read(file, buffer, stats.size);
    VFS::Close(file);

    uint64 pml4Entry;
    IDT::Registers regs;
    if(!PrepareELF(buffer, pml4Entry, regs)) {
        klog_error("Test", "Failed to setup test process");
        delete[] buffer;
        return;
    }

    delete[] buffer;

    uint64 pid = Scheduler::CreateProcess(pml4Entry, &regs);
    klog_info("Test", "Created test process with PID %i", pid);
}

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    Terminal::Init(info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth, info->screenColorsInverted);
    Terminal::Clear();

    klog_info("Kernel", "Kernel at 0x%x", info->kernelImage.buffer);

    MemoryManager::EarlyInit(info);
    GDT::Init(4);
    GDT::InitCore(APIC::GetID());
    IDT::Init();
    IDT::InitCore(APIC::GetID());
    APIC::Init();
    APIC::InitCore();

    SMP::StartCores(info);
    MemoryManager::FreePages(MemoryManager::KernelToPhysPtr(info->smpTrampolineBuffer), info->smpTrampolineBufferPages);

    MemoryManager::Init();

    VFS::Init(new TestFS());
    VFS::CreateFolder("/dev");
    VFS::CreateFolder("/initrd");

    auto ramdriver = new RamDeviceDriver();
    uint64 initrdID = ramdriver->AddDevice((char*)info->ramdiskImage.buffer, 512, info->ramdiskImage.numPages * 8);
    VFS::CreateDeviceFile("/dev/ram0", ramdriver->GetDriverID(), initrdID);

    Ext2::Init();
    VFS::Mount("/initrd", "ext2", "/dev/ram0");

    SetupTestProcess();
    Scheduler::Start();

    while(true) ;
}