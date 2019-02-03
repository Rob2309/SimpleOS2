#include "IDT.h"

#include "types.h"
#include "conio.h"

namespace IDT {
    
    struct __attribute__((packed)) IDTDesc
    {
        uint16 limit;
        uint64 offset;
    };

    struct __attribute__((packed)) IDTEntry
    {
        uint16 offset1;
        uint16 csSelector;
        uint8 zero;
        uint8 typeAttrib;
        uint16 offset2;
        uint32 offset3;
        uint32 reserved;
    };

    struct __attribute__((packed)) Registers
    {
        uint64 ds;
        uint64 rdi, rsi, rbp, rsp, rbx, rdx, rcx, rax;
        uint64 intNumber, errorCode;
        uint64 rip, cs, rflags, userrsp, ss;
    };

    extern "C" void ISR0();
    extern "C" void ISR1();
    extern "C" void ISR2();
    extern "C" void ISR3();
    extern "C" void ISR4();
    extern "C" void ISR5();
    extern "C" void ISR6();
    extern "C" void ISR7();
    extern "C" void ISR8();
    extern "C" void ISR9();
    extern "C" void ISR10();
    extern "C" void ISR11();
    extern "C" void ISR12();
    extern "C" void ISR13();
    extern "C" void ISR14();
    extern "C" void ISR15();
    extern "C" void ISR16();
    extern "C" void ISR17();
    extern "C" void ISR18();
    extern "C" void ISR19();
    extern "C" void ISR20();
    extern "C" void ISR21();
    extern "C" void ISR22();
    extern "C" void ISR23();
    extern "C" void ISR24();
    extern "C" void ISR25();
    extern "C" void ISR26();
    extern "C" void ISR27();
    extern "C" void ISR28();
    extern "C" void ISR29();
    extern "C" void ISR30();
    extern "C" void ISR31();

    extern "C" void ISR100();
    extern "C" void ISR255();
    extern "C" void ISR102();

    IDTEntry g_IDT[256];
    IDTDesc g_IDTDesc;

    uint64 g_APICBase;

    extern "C" void ISRCommonHandler(Registers* regs)
    {
        printf("Interrupt %i\n", regs->intNumber);

        if(regs->intNumber == 100) {
            *(uint32*)(g_APICBase + 0xB0) = 0;

            uint32 m = *(uint32*)(g_APICBase + 0x320);
            if(m & 0x10000) {
                printf("Masked\n");
            }
        }

        if(regs->intNumber == 102) {
            printf("ACIP error\n");
            *(uint32*)(g_APICBase + 0xB0) = 0;
        }
    }

    void SetIDT(uint8 number, void (*vector)(), uint16 selector, uint8 flags)
    {
        g_IDT[number].offset1 = (uint64)vector & 0xFFFF;
        g_IDT[number].csSelector = selector;
        g_IDT[number].zero = 0;
        g_IDT[number].typeAttrib = flags;
        g_IDT[number].offset2 = ((uint64)vector >> 16) & 0xFFFF;
        g_IDT[number].offset3 = ((uint64)vector >> 32) & 0xFFFFFFFF;
    }

    void Init()
    {
        g_IDTDesc.limit = sizeof(g_IDT) - 1;
        g_IDTDesc.offset = (uint64)g_IDT;

        for(uint64 i = 0; i < 256; i++)
            g_IDT[i] = { 0 };

        SetIDT(0, ISR0, 0x08, 0x8E);
        SetIDT(1, ISR1, 0x08, 0x8E);
        SetIDT(2, ISR2, 0x08, 0x8E);
        SetIDT(3, ISR3, 0x08, 0x8E);
        SetIDT(4, ISR4, 0x08, 0x8E);
        SetIDT(5, ISR5, 0x08, 0x8E);
        SetIDT(6, ISR6, 0x08, 0x8E);
        SetIDT(7, ISR7, 0x08, 0x8E);
        SetIDT(8, ISR8, 0x08, 0x8E);
        SetIDT(9, ISR9, 0x08, 0x8E);
        SetIDT(10, ISR10, 0x08, 0x8E);
        SetIDT(11, ISR11, 0x08, 0x8E);
        SetIDT(12, ISR12, 0x08, 0x8E);
        SetIDT(13, ISR13, 0x08, 0x8E);
        SetIDT(14, ISR14, 0x08, 0x8E);
        SetIDT(15, ISR15, 0x08, 0x8E);
        SetIDT(16, ISR16, 0x08, 0x8E);
        SetIDT(17, ISR17, 0x08, 0x8E);
        SetIDT(18, ISR18, 0x08, 0x8E);
        SetIDT(19, ISR19, 0x08, 0x8E);
        SetIDT(20, ISR20, 0x08, 0x8E);
        SetIDT(21, ISR21, 0x08, 0x8E);
        SetIDT(22, ISR22, 0x08, 0x8E);
        SetIDT(23, ISR23, 0x08, 0x8E);
        SetIDT(24, ISR24, 0x08, 0x8E);
        SetIDT(25, ISR25, 0x08, 0x8E);
        SetIDT(26, ISR26, 0x08, 0x8E);
        SetIDT(27, ISR27, 0x08, 0x8E);
        SetIDT(28, ISR28, 0x08, 0x8E);
        SetIDT(29, ISR29, 0x08, 0x8E);
        SetIDT(30, ISR30, 0x08, 0x8E);
        SetIDT(31, ISR31, 0x08, 0x8E);

        SetIDT(100, ISR100, 0x08, 0x8E);
        SetIDT(255, ISR255, 0x08, 0x8E);
        SetIDT(102, ISR102, 0x08, 0x8E);

        DisableInterrupts();

        __asm__ __volatile__ (
            ".intel_syntax noprefix;"
            "lidtq [%0];"
            ".att_syntax prefix;"
            :
            : "r" (&g_IDTDesc)
        );

        EnableInterrupts();
    }

    void EnableInterrupts()
    {
        __asm__ __volatile__ (
            "sti"
        );
    }
    void DisableInterrupts()
    {
        __asm__ __volatile__ (
            "cli"
        );
    }

}