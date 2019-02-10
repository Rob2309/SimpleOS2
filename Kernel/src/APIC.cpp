#include "APIC.h"

#include "conio.h"

#include "IDT.h"

#include "VirtualMemoryManager.h"
#include "PhysicalMemoryManager.h"

namespace APIC
{
    static uint64 g_APICBase;

    constexpr uint64 RegSpurious = 0xF0;
    constexpr uint64 RegError = 0x370;
    constexpr uint64 RegEOI = 0xB0;

    constexpr uint64 RegTimerDiv = 0x3E0;
    constexpr uint64 RegTimerMode = 0x320;
    constexpr uint64 RegTimerInitCount = 0x380;

    static void SendEOI()
    {
        *(uint32*)(g_APICBase + RegEOI) = 0;
    }

    static void ISR_Timer(IDT::Registers* regs)
    {
        printf("Timer interrupt\n");
        SendEOI();
    }
    static void ISR_Error(IDT::Registers* regs)
    {
        printf("APIC Error interrupt\n");
        SendEOI();
    }
    static void ISR_Spurious(IDT::Registers* regs)
    {
    }

    void Init()
    {
        uint32 eax, edx;
        __asm__ __volatile__ (
            "movl $0x1B, %%ecx;"
            "rdmsr"
            : "=a"(eax), "=d"(edx)
            :
            : "ecx"
        );

        g_APICBase = ((uint64)edx << 32) | (eax & 0xFFFFF000);
        printf("APIC Base: 0x%x\n", g_APICBase);

        bool wait = true;
        while(wait);

        VirtualMemoryManager::MapPage(g_APICBase, g_APICBase, true, true);

        IDT::SetISR(ISRNumbers::APICError, ISR_Error);
        IDT::SetISR(ISRNumbers::APICSpurious, ISR_Spurious);
        IDT::SetISR(ISRNumbers::APICTimer, ISR_Timer);

        *(uint32*)(g_APICBase + RegSpurious) = 0x100 | ISRNumbers::APICSpurious;
        *(uint32*)(g_APICBase + RegError) = 0x10000 | ISRNumbers::APICError;
    }
    void StartTimer(uint8 div, uint32 count, bool repeat)
    {
        switch(div)
        {
        case 128: *(uint32*)(g_APICBase + RegTimerDiv) = 0b1010; break;
        }

        *(uint32*)(g_APICBase + RegTimerMode) = (repeat ? 0x20000 : 0) | ISRNumbers::APICTimer;
        *(uint32*)(g_APICBase + RegTimerInitCount) = count;
    }
}