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
#include "arch/SSE.h"
#include "arch/port.h"

namespace SMP {

    extern "C" int smp_Start;
    extern "C" int smp_End;
    extern "C" int smp_TrampolineBaseAddress;
    extern "C" int smp_PML4Address;
    extern "C" int smp_DestinationAddress;
    extern "C" int smp_StackAddress;

    static volatile bool alive = false;
    static volatile bool started = false;
    static volatile uint64 startCount = 0;

    // This function will be called by every secondary CPU core that gets started by StartCores()
    static void CoreEntry() {
        alive = true;

        uint64 logicalID = SMP::GetLogicalCoreID();

        MemoryManager::InitCore();
        Scheduler::MakeMeIdleThread();

        GDT::InitCore(logicalID);
        IDT::InitCore(logicalID);
        APIC::InitCore();
        SyscallHandler::InitCore();
        SSE::InitCore();
        
        started = true;

        __asm__ __volatile__ (
            "lock incq (%0)"
            : : "r"(&startCount)
        );
        
        Scheduler::ThreadUnsetSticky();
        Scheduler::ThreadEnableInterrupts();

        while(true)
            __asm__ __volatile__("hlt");
    }

    struct CoreInfo {
        uint64 apicID;
        uint64 coreID;
    };

    static uint64 g_NumCores;
    static uint64 g_NumRunningCores;
    static CoreInfo g_Info[MaxCoreCount];
    
    void GatherInfo() {
        auto madt = ACPI::GetMADT();

        g_NumCores = 0;
        g_NumRunningCores = 1;

        uint64 pos = 0;
        ACPI::MADTEntryHeader* entry;
        while((entry = madt->GetNextEntry(pos)) != nullptr) {
            if(entry->entryType == ACPI::MADTEntryHeader::TYPE_LAPIC) {
                ACPI::MADTEntryLAPIC* lapic = (ACPI::MADTEntryLAPIC*)(entry);
                
                if(!lapic->processorEnabled) {
                    klog_info_isr("SMP", "LAPIC: Logical=%i, Proc=%i, ID=%i, Flags=%X [DISABLED]", g_NumCores, lapic->processorID, lapic->lapicID, lapic->processorEnabled);
                    continue;
                } else {
                    klog_info_isr("SMP", "LAPIC: Logical=%i, Proc=%i, ID=%i, Flags=%X", g_NumCores, lapic->processorID, lapic->lapicID, lapic->processorEnabled);
                }

                g_Info[g_NumCores].apicID = lapic->lapicID;
                g_Info[g_NumCores].coreID = lapic->processorID;
                g_NumCores++;
            }
        }
    }

    static void Wait(uint16 ms) {
        uint16 count = ms * 1193;

        Port::OutByte(0x43, 0x30);
        Port::OutByte(0x40, count & 0xFF);
        Port::OutByte(0x40, (ms >> 8) & 0xFF);

        while(true) {
            Port::OutByte(0x43, 0xE2);
            uint8 status = Port::InByte(0x40);
            if(status & 0x80)
                break;
        }
    }

    static void WaitSecond() {
        for(int i = 0; i < 100; i++)
            Wait(10);
    }

    void StartCores(uint8* trampolineBuffer, uint64* pageBuffer) {
        // Set up the variables needed by the bootstrap code (startup.asm)
        uint8* buffer = trampolineBuffer;
        kmemcpy(buffer, (void*)&smp_Start, (uint64)&smp_End - (uint64)&smp_Start);

        uint32 addressOffset = (uint64)&smp_TrampolineBaseAddress - (uint64)&smp_Start;
        *(volatile uint64*)(buffer + addressOffset) = (uint64)MemoryManager::KernelToPhysPtr(buffer);

        uint64 entryOffset = (uint64)&smp_DestinationAddress - (uint64)&smp_Start;
        *(volatile uint64*)(buffer + entryOffset) = (uint64)&CoreEntry;

        uint64 pml4Offset = (uint64)&smp_PML4Address - (uint64)&smp_Start;
        *(volatile uint64*)(buffer + pml4Offset) = (uint64)MemoryManager::KernelToPhysPtr(pageBuffer);

        uint64 stackOffset = (uint64)&smp_StackAddress - (uint64)&smp_Start;

        uint64 bootAPIC = APIC::GetID();

        for(uint64 i = 0; i < g_NumCores; i++) {
            if(g_Info[i].apicID == bootAPIC)
                continue;

            klog_info_isr("SMP", "Starting logical Core %i", i);

            uint8* stack = (uint8*)MemoryManager::PhysToKernelPtr(MemoryManager::EarlyAllocatePages(3));
            kmemset(stack, 0, 3 * 4096);
            *(volatile uint64*)(buffer + stackOffset) = (uint64)stack + 3 * 4096;

            alive = false;
            started = false;

            APIC::SendInitIPI(g_Info[i].apicID);
            Wait(10);
            APIC::SendStartupIPI(g_Info[i].apicID, (uint64)MemoryManager::KernelToPhysPtr(buffer));

            Wait(1);

            if(!alive) {
                APIC::SendStartupIPI(g_Info[i].apicID, (uint64)MemoryManager::KernelToPhysPtr(buffer));
                WaitSecond();
                if(!alive) {
                    klog_error_isr("SMP", "Core %i failed to start...", i);
                    continue;
                }
            }

            while(!started) ;   

            g_NumRunningCores++;
            klog_info_isr("SMP", "Logical Core %i started...", i);
        }
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

        return MaxCoreCount;
    }

    uint64 GetApicID(uint64 logicalCore) {
        return g_Info[logicalCore].apicID;
    }

}