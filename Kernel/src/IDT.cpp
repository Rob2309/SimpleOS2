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

    static void ISR_Exceptions(Registers* regs)
    {
        switch (regs->intNumber)
        {
        case ISRNumbers::ExceptionDiv0: printf("Divide by zero error\n"); break;
        case ISRNumbers::ExceptionDebug: printf("Debug trap\n"); break;
        case ISRNumbers::ExceptionNMI: printf("Non maskable interrupt\n"); break;
        case ISRNumbers::ExceptionBreakpoint: printf("Breakpoint\n"); break;
        case ISRNumbers::ExceptionOverflow: printf("Overflow\n"); break;
        case ISRNumbers::ExceptionBoundRangeExceeded: printf("Bound Range exceeded\n"); break;
        case ISRNumbers::ExceptionInvalidOpcode: printf("Invalid opcode\n"); break;
        case ISRNumbers::ExceptionDeviceUnavailable: printf("Device unavailable\n"); break;
        case ISRNumbers::ExceptionDoubleFault: printf("Double fault\n"); break;
        case ISRNumbers::ExceptionCoprocesssorSegmentOverrun: printf("Coprocessor error\n"); break;
        case ISRNumbers::ExceptionInvalidTSS: printf("Invalid TSS\n"); break;
        case ISRNumbers::ExceptionSegmentNotPresent: printf("Segment not present\n"); break;
        case ISRNumbers::ExceptionStackSegmentNotPresent: printf("Stack segment not present\n"); break;
        case ISRNumbers::ExceptionGPFault: printf("General protection fault\n"); break;
        case ISRNumbers::ExceptionPageFault: printf("Page fault\n"); break;
        case ISRNumbers::ExceptionFPException: printf("Floating point exception\n"); break;
        case ISRNumbers::ExceptionAlignmentCheck: printf("Alignment check\n"); break;
        case ISRNumbers::ExceptionMachineCheck: printf("Machine check\n"); break;
        case ISRNumbers::ExceptionSIMDFP: printf("SIMD floating point exception\n"); break;
        case ISRNumbers::ExceptionVirtualization: printf("Virtualization exception\n"); break;
        case ISRNumbers::ExceptionSecurity: printf("Security exception\n"); break;
        }

        uint64 cr2;
        __asm__ __volatile__ (
            "movq %%cr2, %0"
            : "=r"(cr2)
        );
        printf("CR2: 0x%x\n", cr2);
        while(true);
    }

    void Init()
    {
        g_IDTDesc.limit = sizeof(g_IDT) - 1;
        g_IDTDesc.offset = (uint64)g_IDT;

        #undef ISRSTUB
        #undef ISRSTUBE
        #define ISRSTUB(vectno) SetIDTEntry(vectno, ISRSTUB_##vectno, 0x08, 0xEE);
        #define ISRSTUBE(vectno) SetIDTEntry(vectno, ISRSTUB_##vectno, 0x08, 0xEE);
        #include "ISR.inc"

        SetISR(0, ISR_Exceptions);
        SetISR(1, ISR_Exceptions);
        SetISR(2, ISR_Exceptions);
        SetISR(3, ISR_Exceptions);
        SetISR(4, ISR_Exceptions);
        SetISR(5, ISR_Exceptions);
        SetISR(6, ISR_Exceptions);
        SetISR(7, ISR_Exceptions);
        SetISR(8, ISR_Exceptions);
        SetISR(9, ISR_Exceptions);
        SetISR(10, ISR_Exceptions);
        SetISR(11, ISR_Exceptions);
        SetISR(12, ISR_Exceptions);
        SetISR(13, ISR_Exceptions);
        SetISR(14, ISR_Exceptions);
        SetISR(15, ISR_Exceptions);
        SetISR(16, ISR_Exceptions);
        SetISR(17, ISR_Exceptions);
        SetISR(18, ISR_Exceptions);
        SetISR(19, ISR_Exceptions);
        SetISR(20, ISR_Exceptions);
        SetISR(21, ISR_Exceptions);
        SetISR(22, ISR_Exceptions);
        SetISR(23, ISR_Exceptions);
        SetISR(24, ISR_Exceptions);
        SetISR(25, ISR_Exceptions);
        SetISR(26, ISR_Exceptions);
        SetISR(27, ISR_Exceptions);
        SetISR(28, ISR_Exceptions);
        SetISR(29, ISR_Exceptions);
        SetISR(30, ISR_Exceptions);
        SetISR(31, ISR_Exceptions);

        DisableInterrupts();

        __asm__ __volatile__ (
            ".intel_syntax noprefix;"
            "lidtq [%0];"
            ".att_syntax prefix;"
            :
            : "r" (&g_IDTDesc)
        );
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