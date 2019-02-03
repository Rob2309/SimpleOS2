#include "GDT.h"

#include "types.h"
#include "PhysicalMemoryManager.h"
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

    struct __attribute((packed)) GDTDesc
    {
        uint16 size;
        uint64 offset;
    };

    GDTEntry* g_GDT;

    void Init()
    {
        printf("Initializing GDT\n");

        g_GDT = (GDTEntry*)PhysicalMemoryManager::AllocatePages(5 * sizeof(GDTEntry));
        printf("GDT at 0x%x\n", g_GDT);

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

        GDTDesc desc = { 5 * sizeof(GDTEntry) - 1, (uint64)(&g_GDT[0]) };
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
    }
}