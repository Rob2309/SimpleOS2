#include "GDT.h"

#include "types.h"
#include "allocate.h"
#include "conio.h"

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

    static GDTEntry g_GDT[7];
    static TSS g_TSS;

    static uint8 g_InterruptStack[4096];

    void Init()
    {
        printf("Initializing GDT\n");

        // null descriptor
        g_GDT[0] = { 0 };

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
        g_GDT[2].limit2flags = 0b10101111;
        g_GDT[2].base3 = 0x00;

        // user code
        g_GDT[3].limit1 = 0xFFFF;
        g_GDT[3].base1 = 0x0000;
        g_GDT[3].base2 = 0x00;
        g_GDT[3].access = 0b11111010;
        g_GDT[3].limit2flags = 0b10101111;
        g_GDT[3].base3 = 0x00;

        // user data
        g_GDT[4].limit1 = 0xFFFF;
        g_GDT[4].base1 = 0x0000;
        g_GDT[4].base2 = 0x00;
        g_GDT[4].access = 0b11110010;
        g_GDT[4].limit2flags = 0b10101111;
        g_GDT[4].base3 = 0x00;

        g_TSS = { 0 };
        g_TSS.iopbOffset = sizeof(TSS);
        g_TSS.rsp0 = (uint64)&g_InterruptStack[0];

        TSSDesc* tssDesc = (TSSDesc*)&g_GDT[5];
        *tssDesc = { 0 };
        tssDesc->limit1 = sizeof(TSS) & 0xFFFF;
        tssDesc->limit2flags = (sizeof(TSS) >> 16) & 0xF;
        tssDesc->base1 = (uint64)&g_TSS & 0xFFFF;
        tssDesc->base2 = ((uint64)&g_TSS >> 16) & 0xFF;
        tssDesc->base3 = ((uint64)&g_TSS >> 24) & 0xFF;
        tssDesc->base4 = ((uint64)&g_TSS >> 32) & 0xFFFFFFFF;
        tssDesc->access = 0b11101001;

        GDTDesc desc = { sizeof(g_GDT) - 1, (uint64)(&g_GDT[0]) };
        __asm__ __volatile__ (
            ".intel_syntax noprefix;"
            "lgdt [%0];"                // tell cpu to use new GDT
            "mov rax, 16;"              // kernel data selector
            "mov ds, rax;"
            "mov es, rax;"
            "mov fs, rax;"
            "mov gs, rax;"
            "mov ss, rax;"
            "pushq 8;"                  // kernel code selector
            "leaq rax, [rip + 1f];"     // rax = address of "1" label below
            "pushq rax;"
            "retfq;"                    // pops return address and cs
            "1: nop;"
            ".att_syntax prefix"
            : 
            : "r" (&desc)               // %0 = &desc in some register
            : "rax"                     // rax is changed in this __asm__ block
        );

        __asm__ __volatile__ (
            "ltr %%ax"
            : : "a" (5 * 8)
        );
    }
}