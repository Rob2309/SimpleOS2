#include "IDT.h"

#include "types.h"
#include "conio.h"


#define ISRSTUB(vectno) extern "C" void ISRSTUB_##vectno();
#define ISRSTUBE(vectno) extern "C" void ISRSTUB_##vectno();
#include "ISR.inc"

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

    static IDTEntry g_IDT[256] = { 0 };
    static IDTDesc g_IDTDesc;

    static ISR g_Handlers[256] = { 0 };

    extern "C" void ISRCommonHandler(Registers* regs)
    {
        if(g_Handlers[regs->intNumber] == nullptr)
            printf("INVALID INTERRUPT: %i\n", regs->intNumber);
        else
            g_Handlers[regs->intNumber](regs);
    }

    static void SetIDTEntry(uint8 number, void (*vector)(), uint16 selector, uint8 flags)
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

        #undef ISRSTUB
        #undef ISRSTUBE
        #define ISRSTUB(vectno) SetIDTEntry(vectno, ISRSTUB_##vectno, 0x08, 0x8E);
        #define ISRSTUBE(vectno) SetIDTEntry(vectno, ISRSTUB_##vectno, 0x08, 0x8E);
        #include "ISR.inc"

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

    void SetISR(uint8 index, ISR isr)
    {
        g_Handlers[index] = isr;
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