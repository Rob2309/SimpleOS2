#include "SMP.h"

#include "acpi/ACPI.h"
#include "memory/MemoryManager.h"
#include "klib/stdio.h"
#include "arch/APIC.h"
#include "klib/memory.h"

namespace SMP {

    extern "C" int smp_Start;
    extern "C" int smp_End;
    extern "C" int smp_Trampoline;
    extern "C" int smp_StartupFlag;
    extern "C" int smp_Address;
    extern "C" int smp_PML4Address;

    static volatile bool wait = true;

    static void ISR_Timer(IDT::Registers* regs) {
        wait = false;
    }

    void StartCores(KernelHeader* header) {
        ACPI::RSDPDescriptor* rsdp = (ACPI::RSDPDescriptor*)header->rsdp;
        ACPI::XSDT* xsdt = (ACPI::XSDT*)MemoryManager::PhysToKernelPtr((void*)rsdp->xsdtAddress);
        klog_info("ACPI", "XSDT at 0x%x", xsdt);

        ACPI::MADT* madt = (ACPI::MADT*)xsdt->FindTable(SIGNATURE('A', 'P', 'I', 'C'));
        if(madt == nullptr) {
            klog_fatal("ACPI", "MADT not found");
            return;
        }
        else
            klog_info("ACPI", "MADT at 0x%x", madt);

        APIC::SetTimerEvent(ISR_Timer);
        
        uint8* buffer = header->smpTrampolineBuffer;
        kmemcpy(buffer, (void*)&smp_Start, (uint64)&smp_End - (uint64)&smp_Start);

        uint32 addressOffset = (uint64)&smp_Address - (uint64)&smp_Start;
        *(volatile uint32*)(buffer + addressOffset) = (uint64)MemoryManager::KernelToPhysPtr(buffer);

        uint64 flagOffset = (uint64)&smp_StartupFlag - (uint64)&smp_Start;
        volatile uint16* flag = (uint16*)(buffer + flagOffset);

        uint64 pml4Offset = (uint64)&smp_PML4Address - (uint64)&smp_Start;
        *(volatile uint64*)(buffer + pml4Offset) = (uint64)MemoryManager::KernelToPhysPtr(header->pageBuffer);

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
    }

}