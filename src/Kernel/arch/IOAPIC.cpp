#include "IOAPIC.h"

#include "acpi/ACPI.h"
#include "memory/MemoryManager.h"
#include "klib/stdio.h"
#include "interrupts/ISRNumbers.h"
#include "multicore/SMP.h"

namespace IOAPIC {

    #define REG_SEL 0x00
    #define REG_DATA 0x10

    #define REG_ID 0
    #define REG_VER 1
    #define REG_IRQ_BASE 0x10

    static uint8 g_ISRCounter = ISRNumbers::IRQBase;
    static uint32 g_GSIMapping[1024] = { 0 };

    static uint64 g_MemBase;

    void Init(KernelHeader* header) {
        ACPI::RSDPDescriptor* rsdp = (ACPI::RSDPDescriptor*)header->rsdp;
        ACPI::XSDT* xsdt = (ACPI::XSDT*)MemoryManager::PhysToKernelPtr((void*)rsdp->xsdtAddress);
        klog_info("IOAPIC", "XSDT at 0x%016X", xsdt);

        ACPI::MADT* madt = (ACPI::MADT*)xsdt->FindTable(SIGNATURE('A', 'P', 'I', 'C'));
        if(madt == nullptr) {
            klog_fatal("IOAPIC", "MADT not found");
            return;
        }
        else
            klog_info("IOAPIC", "MADT at 0x%016X", madt);

        uint64 pos = 0;
        ACPI::MADTEntryHeader* entry;
        while((entry = madt->GetNextEntry(pos)) != nullptr) {
            if(entry->entryType == ACPI::MADTEntryHeader::TYPE_IOAPIC) {
                auto ioapic = (ACPI::MADTEntryIOAPIC*)entry;
                g_MemBase = (uint64)MemoryManager::PhysToKernelPtr((void*)(uint64)ioapic->ioApicAddr);
                klog_info("IOAPIC", "Found IOAPIC at 0x%016X (gsiBase=%i)", g_MemBase, ioapic->gsiBase);
            } else if(entry->entryType == ACPI::MADTEntryHeader::TYPE_ISO) {
                auto iso = (ACPI::MADTEntryISO*)entry;
                klog_info("IOAPIC", "ISO gsi=%i bus=%i irq=%i", iso->gsi, iso->bus, iso->irq);
                g_GSIMapping[iso->irq] = iso->gsi;
            }
        }
    }

    void RegisterIRQ(uint8 irq, IDT::ISR isr) {
        uint32 gsi = g_GSIMapping[irq];
        if(gsi == 0) {
            klog_error("IOAPIC", "IRQ %i has no GSI mapping, cannot register irq handler", irq);
            return;
        }

        uint8 intNo = g_ISRCounter++;
        IDT::SetISR(intNo, isr);

        uint32 valA = 0, valB = 0;
        valA |= intNo;

        uint64 apicID = SMP::GetApicID(SMP::GetLogicalCoreID());
        valB |= apicID << 24;

        uint32 reg = REG_IRQ_BASE + 2 * gsi;
        *(volatile uint32*)(g_MemBase + REG_SEL) = reg;
        *(volatile uint32*)(g_MemBase + REG_DATA) = valA;
        *(volatile uint32*)(g_MemBase + REG_SEL) = reg + 1;
        *(volatile uint32*)(g_MemBase + REG_DATA) = valB;

        klog_info("IOAPIC", "Routing IRQ %i through GSI %i", irq, gsi);
    }

}