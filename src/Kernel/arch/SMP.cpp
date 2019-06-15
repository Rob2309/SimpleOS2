#include "SMP.h"

#include "acpi/ACPI.h"
#include "memory/MemoryManager.h"
#include "klib/stdio.h"
#include "arch/APIC.h"
#include "klib/memory.h"

namespace SMP {

    extern "C" void func_CoreStartup();
    extern "C" void func_CoreStartup_End();

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

        volatile uint16* flag = (uint16*)MemoryManager::PhysToKernelPtr((void*)0x1234);
        
        uint8* buffer = header->smpTrampolineBuffer;
        kmemcpy(buffer, (void*)&func_CoreStartup, (uint64)&func_CoreStartup_End - (uint64)&func_CoreStartup);

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