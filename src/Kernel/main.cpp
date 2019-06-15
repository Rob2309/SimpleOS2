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

#include "acpi/ACPI.h"

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

static VFS::FileSystem* TestFactory() {
    return new TestFS();
}

static volatile bool wait = true;
static void ISR_Timer(IDT::Registers* regs) {
    wait = false;
}

extern "C" void func_CoreStartup();
extern "C" void func_CoreStartup_End();

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    Terminal::Init(info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth, info->screenColorsInverted);
    Terminal::Clear();

    klog_info("Kernel", "Kernel at 0x%x", info->kernelImage.buffer);
    
    MemoryManager::Init(info);

    GDT::Init(info);
    IDT::Init();
    SyscallHandler::Init();

    Ext2::Init();
    VFS::FileSystemRegistry::RegisterFileSystem("test", TestFactory);

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

    char* interruptStack = (char*)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages(10));
    GDT::SetIST1(interruptStack + 10 * 4096);

    volatile uint16* flag = (uint16*)MemoryManager::PhysToKernelPtr((void*)0x1234);
    uint8* buffer = info->smpTrampolineBuffer;
    kmemcpy(buffer, (void*)&func_CoreStartup, (uint64)&func_CoreStartup_End - (uint64)&func_CoreStartup);

    APIC::SetTimerEvent(ISR_Timer);

    ACPI::RSDPDescriptor* rsdp = (ACPI::RSDPDescriptor*)info->rsdp;
    ACPI::XSDT* xsdt = (ACPI::XSDT*)MemoryManager::PhysToKernelPtr((void*)rsdp->xsdtAddress);
    klog_info("ACPI", "XSDT at 0x%x", xsdt);

    ACPI::MADT* madt = (ACPI::MADT*)xsdt->FindTable(SIGNATURE('A', 'P', 'I', 'C'));
    if(madt == nullptr)
        klog_fatal("ACPI", "MADT not found");
    else
        klog_info("ACPI", "MADT at 0x%x", madt);

    uint64 pos = 0;
    ACPI::MADTEntryHeader* entry;
    while((entry = madt->GetNextEntry(pos)) != nullptr) {
        if(entry->entryType == ACPI::MADTEntryHeader::TYPE_LAPIC) {
            ACPI::MADTEntryLAPIC* lapic = (ACPI::MADTEntryLAPIC*)(entry);
            klog_info("SMP", "LAPIC: Proc=%i, ID=%i, Flags=%X", lapic->processorID, lapic->lapicID, lapic->processorEnabled);

            if(lapic->lapicID == 0)
                continue;

            wait = true;
            *flag = 0;
            APIC::SendInitIPI(lapic->lapicID);
            APIC::StartTimer(10);
            IDT::EnableInterrupts();
            while(wait) ;
            IDT::DisableInterrupts();
            APIC::SendStartupIPI(lapic->lapicID, (uint64)MemoryManager::KernelToPhysPtr(buffer));

            while(*flag != 0xDEAD) ;
            klog_info("SMP", "Core %i started...", lapic->processorID);
        }
    }

    SetupTestProcess((uint8*)0x16000);
    Scheduler::Start();

    // should never be reached
    while(true) {
        __asm__ __volatile__ (
            "hlt"
        );
    }

}