#include "GDT.h"

#include "types.h"
#include "klib/stdio.h"
#include "klib/memory.h"
#include "memory/MemoryManager.h"

namespace GDT
{
    struct __attribute__((packed)) GDTEntry
    {
        uint16 limit1;
        uint16 base1;
        uint8 base2;
        uint8 access;
        uint8 limit2flags;
        uint8 base3;
    };

    struct __attribute__((packed)) TSSDesc
    {
        uint16 limit1;
        uint16 base1;
        uint8 base2;
        uint8 access;
        uint8 limit2flags;
        uint8 base3;
        uint32 base4;
        uint32 reserved;
    };

    struct __attribute__((packed)) TSS
    {
        uint32 reserved;
        uint64 rsp0;
        uint64 rsp1;
        uint64 rsp2;
        uint64 reserved2;
        uint64 ist1;
        uint64 ist2;
        uint64 ist3;
        uint64 ist4;
        uint64 ist5;
        uint64 ist6;
        uint64 ist7;
        uint64 reserved3;
        uint16 unused;
        uint16 iopbOffset;
    };

    struct __attribute__((packed)) GDTDesc
    {
        uint16 size;
        uint64 offset;
    };

    static volatile GDTEntry* g_GDT;
    static volatile TSS* g_TSS;
    static GDTDesc g_GDTDesc;

    void Init(uint64 coreCount)
    {
        klog_info_isr("GDT", "Reserving space for GDT");
        
        g_GDT = (GDTEntry*)MemoryManager::PhysToKernelPtr(MemoryManager::EarlyAllocatePages(((5 + coreCount * 2) * sizeof(GDTEntry) + 4095) / 4096));
        g_TSS = (TSS*)MemoryManager::PhysToKernelPtr(MemoryManager::EarlyAllocatePages((coreCount * sizeof(TSS) + 4095) / 4096));

        // null descriptor
        kmemset((void*)&g_GDT[0], 0, sizeof(GDTEntry));

        // kernel code
        g_GDT[1].limit1 = 0xFFFF;
        g_GDT[1].base1 = 0x0000;
        g_GDT[1].base2 = 0x00;
        g_GDT[1].access = 0b10011010;
        g_GDT[1].limit2flags = 0b10101111;
        g_GDT[1].base3 = 0x00;

        // kernel data
        g_GDT[2].limit1 = 0xFFFF;
        g_GDT[2].base1 = 0x0000;
        g_GDT[2].base2 = 0x00;
        g_GDT[2].access = 0b10010010;
        g_GDT[2].limit2flags = 0b10001111;
        g_GDT[2].base3 = 0x00;

        // user data
        g_GDT[3].limit1 = 0xFFFF;
        g_GDT[3].base1 = 0x0000;
        g_GDT[3].base2 = 0x00;
        g_GDT[3].access = 0b11110010;
        g_GDT[3].limit2flags = 0b10001111;
        g_GDT[3].base3 = 0x00;

        // user code
        g_GDT[4].limit1 = 0xFFFF;
        g_GDT[4].base1 = 0x0000;
        g_GDT[4].base2 = 0x00;
        g_GDT[4].access = 0b11111010;
        g_GDT[4].limit2flags = 0b10101111;
        g_GDT[4].base3 = 0x00;

        g_GDTDesc.size = (5 + coreCount * 2) * sizeof(GDTEntry) - 1;
        g_GDTDesc.offset = (uint64)g_GDT;
    }

    void InitCore(uint64 coreID) {
        // TSS, needed for inter priviledge level interrupts
        kmemset((void*)&g_TSS[coreID], 0, sizeof(TSS));
        
        g_TSS[coreID].iopbOffset = sizeof(TSS);
        // this is the stack pointer that will be loaded when an interrupt CHANGES the priviledge level to 0
        // if an interrupt is fired in kernel mode, this stack pointer won't be used
        g_TSS[coreID].rsp0 = 0;
        // This is a stack that can be explicitly enabled for specific interrupts.
        // Those interrupts will then always use this stack, no matter in which privilege level it occured
        //g_TSS.ist1 = (uint64)header->stack + header->stackPages * 4096;
        g_TSS[coreID].ist1 = 0;

        volatile TSSDesc* tssDesc = (volatile TSSDesc*)&g_GDT[5 + coreID * 2];
        kmemset((void*)tssDesc, 0, sizeof(TSSDesc));
        tssDesc->limit1 = (sizeof(TSS) - 1) & 0xFFFF;
        tssDesc->limit2flags = ((sizeof(TSS) - 1) >> 16) & 0xF;
        tssDesc->base1 = (uint64)&g_TSS[coreID] & 0xFFFF;
        tssDesc->base2 = ((uint64)&g_TSS[coreID] >> 16) & 0xFF;
        tssDesc->base3 = ((uint64)&g_TSS[coreID] >> 24) & 0xFF;
        tssDesc->base4 = ((uint64)&g_TSS[coreID] >> 32) & 0xFFFFFFFF;
        tssDesc->access = 0b11101001;

        __asm__ __volatile__ (
            "lgdtq (%0);"                    // tell cpu to use new GDT
            "mov $0x10, %%rax;"               // kernel data selector
            "mov %%ax, %%ds;"
            "mov %%ax, %%es;"
            "mov %%ax, %%ss;"
            "pushq $0x08;"                     // kernel code selector
            "leaq 1f(%%rip), %%rax;"        // rax = address of "1" label below
            "pushq %%rax;"
            "retfq;"                        // pops return address and cs
            "1: nop;"
            : 
            : "r" (&g_GDTDesc)
            : "rax"
        );

        __asm__ __volatile__ (
            "ltr %%ax"              // tell cpu to use new TSS
            : : "a" ((5 + coreID * 2) * sizeof(GDTEntry))
        );
    }

    void SetIST1(uint64 coreID, void* addr) {
        g_TSS[coreID].ist1 = (uint64)addr;
    }
}