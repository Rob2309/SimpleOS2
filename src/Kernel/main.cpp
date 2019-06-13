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

    ACPI::RSDPDescriptor* rsdp = (ACPI::RSDPDescriptor*)info->rsdp;
    ACPI::XSDT* xsdt = (ACPI::XSDT*)MemoryManager::PhysToKernelPtr((void*)rsdp->xsdtAddress);
    klog_info("ACPI", "XSDT at 0x%x", xsdt);

    ACPI::MADT* madt = nullptr;
    uint64 numEntries = (xsdt->header.length - sizeof(ACPI::XSDT)) / 8;
    for(int i = 0; i < numEntries; i++) {
        ACPI::SDTHeader* entry = (ACPI::SDTHeader*)MemoryManager::PhysToKernelPtr(xsdt->tables[i]);
        if(entry->signature[0] = 'A' && entry->signature[1] == 'P' && entry->signature[2] == 'I' && entry->signature[3] == 'C') {
            madt = (ACPI::MADT*)entry;
            break;
        }
    }
    if(madt == nullptr)
        klog_fatal("ACPI", "MADT not found");
    else
        klog_info("ACPI", "MADT at 0x%x", madt);

    uint64 pos = 0;
    while(pos < madt->header.length - sizeof(ACPI::MADT)) {
        ACPI::MADTEntryHeader* entry = (ACPI::MADTEntryHeader*)(&madt->entries[pos]);
        if(entry->entryType == ACPI::MADTEntryHeader::TYPE_LAPIC) {
            ACPI::MADTEntryLAPIC* lapic = (ACPI::MADTEntryLAPIC*)(entry);
            kprintf("LAPIC: Proc=%i, ID=%i, Flags=%X\n", lapic->processorID, lapic->lapicID, lapic->processorEnabled);
        }

        pos += entry->length;
    }

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