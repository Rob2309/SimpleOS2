#include "SMP.h"

#include "acpi/ACPI.h"
#include "memory/MemoryManager.h"
#include "klib/stdio.h"
#include "arch/GDT.h"
#include "interrupts/IDT.h"
#include "arch/APIC.h"
#include "klib/memory.h"
#include "syscalls/SyscallHandler.h"
#include "scheduler/Scheduler.h"

namespace SMP {

    extern "C" int smp_Start;
    extern "C" int smp_End;
    extern "C" int smp_TrampolineBaseAddress;
    extern "C" int smp_PML4Address;
    extern "C" int smp_DestinationAddress;
    extern "C" int smp_StackAddress;

    static volatile bool wait = true;
    static volatile bool started = false;
    static volatile bool startScheduler = false;
    static volatile uint64 startCount = 0;

    static void ISR_Timer(IDT::Registers* regs) {
        wait = false;
    }

    static void TestThread() {
        while(true) {
            kprintf("Thread alive on Core %i\n", SMP::GetLogicalCoreID());
            Scheduler::ThreadWait(1000);
        }
    }

    static void CoreEntry() {
        GDT::InitCore(SMP::GetLogicalCoreID());
        IDT::InitCore(SMP::GetLogicalCoreID());
        APIC::InitCore();
        MemoryManager::InitCore(SMP::GetLogicalCoreID());
        SyscallHandler::InitCore();

        Scheduler::CreateKernelThread((uint64)&TestThread);

        started = true;
        
        while(!startScheduler) ;

        IDT::DisableInterrupts();
        __asm__ __volatile__ (
            "lock incq (%0)"
            : : "r"(&startCount)
        );
        Scheduler::Start();
    }

    struct CoreInfo {
        uint64 apicID;
        uint64 coreID;
    };

    static uint64 g_NumCores;
    static CoreInfo g_Info[MaxCoreCount];

    static uint8* g_TrampolineBuffer;
    static uint64* g_PageBuffer;
    
    void GatherInfo(KernelHeader* header) {
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

        g_NumCores = 0;

        uint64 pos = 0;
        ACPI::MADTEntryHeader* entry;
        while((entry = madt->GetNextEntry(pos)) != nullptr) {
            if(entry->entryType == ACPI::MADTEntryHeader::TYPE_LAPIC) {
                ACPI::MADTEntryLAPIC* lapic = (ACPI::MADTEntryLAPIC*)(entry);
                
                if(!lapic->processorEnabled) {
                    klog_info("SMP", "LAPIC: Proc=%i, ID=%i, Flags=%X [DISABLED]", lapic->processorID, lapic->lapicID, lapic->processorEnabled);
                    continue;
                } else {
                    klog_info("SMP", "LAPIC: Proc=%i, ID=%i, Flags=%X", lapic->processorID, lapic->lapicID, lapic->processorEnabled);
                }

                g_Info[g_NumCores].apicID = lapic->lapicID;
                g_Info[g_NumCores].coreID = lapic->processorID;
                g_NumCores++;
            }
        }

        g_TrampolineBuffer = header->smpTrampolineBuffer;
        g_PageBuffer = header->pageBuffer;
    }

    void StartCores() {
        APIC::SetTimerEvent(ISR_Timer);
        
        uint8* buffer = g_TrampolineBuffer;
        kmemcpy(buffer, (void*)&smp_Start, (uint64)&smp_End - (uint64)&smp_Start);

        uint32 addressOffset = (uint64)&smp_TrampolineBaseAddress - (uint64)&smp_Start;
        *(volatile uint64*)(buffer + addressOffset) = (uint64)MemoryManager::KernelToPhysPtr(buffer);

        uint64 entryOffset = (uint64)&smp_DestinationAddress - (uint64)&smp_Start;
        *(volatile uint64*)(buffer + entryOffset) = (uint64)&CoreEntry;

        uint64 pml4Offset = (uint64)&smp_PML4Address - (uint64)&smp_Start;
        *(volatile uint64*)(buffer + pml4Offset) = (uint64)MemoryManager::KernelToPhysPtr(g_PageBuffer);

        uint64 stackOffset = (uint64)&smp_StackAddress - (uint64)&smp_Start;

        uint64 bootAPIC = APIC::GetID();

        for(int i = 0; i < g_NumCores; i++) {
            if(g_Info[i].apicID == bootAPIC)
                continue;

            klog_info("SMP", "Starting logical Core %i", i);

            *(volatile uint64*)(buffer + stackOffset) = (uint64)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages(3));

            wait = true;
            started = false;
            APIC::SendInitIPI(g_Info[i].apicID);
            APIC::StartTimer(10);
            IDT::EnableInterrupts();
            while(wait) ;
            IDT::DisableInterrupts();
            APIC::SendStartupIPI(g_Info[i].apicID, (uint64)MemoryManager::KernelToPhysPtr(buffer));

            while(!started) ;
            klog_info("SMP", "Logical Core %i started...", i);
        }

        startScheduler = true;

        while(startCount < SMP::GetCoreCount() - 1) ;
    }

    uint64 GetCoreCount() {
        return g_NumCores;
    }

    uint64 GetLogicalCoreID() {
        uint64 lapicID = APIC::GetID();
        for(uint64 i = 0; i < g_NumCores; i++) {
            if(g_Info[i].apicID == lapicID)
                return i;
        }
    }

    uint64 GetApicID(uint64 logicalCore) {
        return g_Info[logicalCore].apicID;
    }

}