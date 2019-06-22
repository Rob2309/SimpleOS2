#include "KernelHeader.h"

#include "terminal/terminal.h"
#include "klib/stdio.h"
#include "arch/GDT.h"
#include "interrupts/IDT.h"
#include "arch/APIC.h"
#include "syscalls/SyscallHandler.h"
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

static void TestThread() {
    while(true) {
        kprintf("Thread alive on Core %i\n", SMP::GetCoreID());
        Scheduler::ThreadWait(1000);
    }
}

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    Terminal::Init(info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth, info->screenColorsInverted);
    Terminal::Clear();

    klog_info("Kernel", "Kernel at 0x%x", info->kernelImage.buffer);

    MemoryManager::Init(info, 20);
    APIC::Init();
    SMP::GatherInfo(info);
    GDT::Init(SMP::GetCoreCount());
    GDT::InitCore(SMP::GetCoreID());
    IDT::Init();
    IDT::InitCore(SMP::GetCoreID());
    APIC::InitBootCore();
    SyscallHandler::InitCore();

    Scheduler::Init(SMP::GetCoreCount());

    SMP::StartCores();
    MemoryManager::FreePages(MemoryManager::KernelToPhysPtr(info->smpTrampolineBuffer), info->smpTrampolineBufferPages);

    VFS::Init(new TestFS());
    VFS::CreateFolder("/dev");
    VFS::CreateFolder("/initrd");

    auto ramdriver = new RamDeviceDriver();
    uint64 initrdID = ramdriver->AddDevice((char*)info->ramdiskImage.buffer, 512, info->ramdiskImage.numPages * 8);
    VFS::CreateDeviceFile("/dev/ram0", ramdriver->GetDriverID(), initrdID);

    Ext2::Init();
    VFS::Mount("/initrd", "ext2", "/dev/ram0");

    //SetupTestProcess();

    Scheduler::CreateKernelThread((uint64)&TestThread);

    Scheduler::Start();

    while(true) ;
}