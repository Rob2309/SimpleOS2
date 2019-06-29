#include "IDT.h"

#include "types.h"
#include "klib/stdio.h"
#include "arch/GDT.h"
#include "scheduler/Process.h"
#include "memory/MemoryManager.h"
#include "arch/APIC.h"

#define ISRSTUB(vectno) extern "C" void ISRSTUB_##vectno();
#define ISRSTUBE(vectno) extern "C" void ISRSTUB_##vectno();
#include "ISR.inc"

#include "scheduler/Scheduler.h"

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
        uint8 ist;
        uint8 typeAttrib;
        uint16 offset2;
        uint32 offset3;
        uint32 reserved;
    };

    static volatile IDTEntry g_IDT[256] = { 0 };
    static volatile IDTDesc g_IDTDesc;

    static ISR g_Handlers[256] = { 0 };

    extern "C" void ISRCommonHandler(Registers* regs)
    {
        if(g_Handlers[regs->intNumber] == nullptr)
            klog_error("IDT", "INVALID INTERRUPT: %i", regs->intNumber);
        else
            g_Handlers[regs->intNumber](regs);
    }

    static void SetIDTEntry(uint8 number, void (*vector)(), uint16 selector, uint8 flags)
    {
        g_IDT[number].offset1 = (uint64)vector & 0xFFFF;
        g_IDT[number].csSelector = selector;
        g_IDT[number].ist = 1;  // use ist1 stack for this interrupt (see GDT.cpp)
        g_IDT[number].typeAttrib = flags;
        g_IDT[number].offset2 = ((uint64)vector >> 16) & 0xFFFF;
        g_IDT[number].offset3 = ((uint64)vector >> 32) & 0xFFFFFFFF;
    }

    static void ISR_Exceptions(Registers* regs)
    {
        switch (regs->intNumber)
        {
        case ISRNumbers::ExceptionDiv0: Scheduler::ThreadKillProcessFromInterrupt(regs); return; break;
        case ISRNumbers::ExceptionDebug: klog_error("IDT", "Debug trap"); break;
        case ISRNumbers::ExceptionNMI: klog_error("IDT", "Non maskable interrupt"); break;
        case ISRNumbers::ExceptionBreakpoint: klog_error("IDT", "Breakpoint"); break;
        case ISRNumbers::ExceptionOverflow: klog_error("IDT", "Overflow"); break;
        case ISRNumbers::ExceptionBoundRangeExceeded: klog_error("IDT", "Bound Range exceeded"); break;
        case ISRNumbers::ExceptionInvalidOpcode: Scheduler::ThreadKillProcessFromInterrupt(regs); return; break;
        case ISRNumbers::ExceptionDeviceUnavailable: klog_error("IDT", "Device unavailable"); break;
        case ISRNumbers::ExceptionDoubleFault: klog_error("IDT", "Double fault"); break;
        case ISRNumbers::ExceptionCoprocesssorSegmentOverrun: klog_error("IDT", "Coprocessor error"); break;
        case ISRNumbers::ExceptionInvalidTSS: klog_error("IDT", "Invalid TSS"); break;
        case ISRNumbers::ExceptionSegmentNotPresent: klog_error("IDT", "Segment not present"); break;
        case ISRNumbers::ExceptionStackSegmentNotPresent: klog_error("IDT", "Stack segment not present"); break;
        case ISRNumbers::ExceptionGPFault: Scheduler::ThreadKillProcessFromInterrupt(regs); return; break;
        case ISRNumbers::ExceptionPageFault: Scheduler::ThreadKillProcessFromInterrupt(regs); return; break;
        case ISRNumbers::ExceptionFPException: klog_error("IDT", "Floating point exception"); break;
        case ISRNumbers::ExceptionAlignmentCheck: klog_error("IDT", "Alignment check"); break;
        case ISRNumbers::ExceptionMachineCheck: klog_error("IDT", "Machine check"); break;
        case ISRNumbers::ExceptionSIMDFP: klog_error("IDT", "SIMD floating point exception"); break;
        case ISRNumbers::ExceptionVirtualization: klog_error("IDT", "Virtualization exception"); break;
        case ISRNumbers::ExceptionSecurity: klog_error("IDT", "Security exception"); break;
        }

        uint64 cr2;
        __asm__ __volatile__ (
            "movq %%cr2, %0"
            : "=r"(cr2)
        );
        klog_fatal("PANIC", "Unhandled interrupt %i: CR2=0x%x, ErrorCode=0x%X, RIP=0x%x", regs->intNumber, cr2, regs->errorCode, regs->rip);
        while(true);
    }

    void Init()
    {
        g_IDTDesc.limit = sizeof(g_IDT) - 1;
        g_IDTDesc.offset = (uint64)g_IDT;

        #undef ISRSTUB
        #undef ISRSTUBE
        #define ISRSTUB(vectno) SetIDTEntry(vectno, ISRSTUB_##vectno, GDT::KernelCode, 0x8E);
        #define ISRSTUBE(vectno) SetIDTEntry(vectno, ISRSTUB_##vectno, GDT::KernelCode, 0x8E);
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
    }

    void InitCore(uint64 coreID) {
        uint8* interruptBuffer = (uint8*)MemoryManager::PhysToKernelPtr(MemoryManager::EarlyAllocatePages(4));
        GDT::SetIST1(coreID, interruptBuffer + 4 * 4096);

        DisableInterrupts();

        __asm__ __volatile__ (
            ".intel_syntax noprefix;"
            "lidtq [%0];"               // tell cpu to use new IDT
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