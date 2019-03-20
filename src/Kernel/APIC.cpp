#include "APIC.h"

#include "conio.h"

#include "MemoryManager.h"

#include "port.h"

namespace APIC
{
    constexpr uint64 RegSpurious = 0xF0;
    constexpr uint64 RegError = 0x370;
    constexpr uint64 RegEOI = 0xB0;

    constexpr uint64 RegTimerDiv = 0x3E0;
    constexpr uint64 RegTimerMode = 0x320;
    constexpr uint64 RegTimerCurrentCount = 0x390;
    constexpr uint64 RegTimerInitCount = 0x380;

    static uint64 g_APICBase;
    static uint64 g_TimerTicksPerMS;
    static TimerEvent g_TimerEvent = nullptr;

    static void SendEOI()
    {
        *(volatile uint32*)(g_APICBase + RegEOI) = 0;
    }

    static void ISR_Timer(IDT::Registers* regs)
    {
        if(g_TimerEvent != nullptr)
            g_TimerEvent(regs);
        SendEOI();
    }
    static void ISR_Error(IDT::Registers* regs)
    {
        SendEOI();
    }
    static void ISR_Spurious(IDT::Registers* regs)
    {
    }

    static void CalibrateTimer()
    {
        Port::OutByte(0x43, 0x30);

        StartTimer(1, 0xFFFFFFFF, false);

        constexpr uint16 pitTickInitCount = 39375; // 33 ms
        Port::OutByte(0x40, pitTickInitCount & 0xFF);
        Port::OutByte(0x40, (pitTickInitCount >> 8) & 0xFF);

        uint16 pitCurrentCount = 0;
        while(pitCurrentCount <= pitTickInitCount)
        {
            Port::OutByte(0x43, 0);
            pitCurrentCount = (uint16)Port::InByte(0x40) | ((uint16)Port::InByte(0x40) << 8);
        }

        uint32 apicTimerRemaining = *(volatile uint32*)(g_APICBase + RegTimerCurrentCount);
        uint32 elapsed = 0xFFFFFFFF - apicTimerRemaining;
        g_TimerTicksPerMS = elapsed / 33;
        printf("APIC Timer runs at %i kHz\n", g_TimerTicksPerMS);
    }

    void SetTimerEvent(TimerEvent evt)
    {
        g_TimerEvent = evt;
    }

    void Init()
    {
        uint32 eax, edx;
        __asm__ __volatile__ (
            "movl $0x1B, %%ecx;"    // msr 0x1B contains the physical address of the APIC registers
            "rdmsr"
            : "=a"(eax), "=d"(edx)
            :
            : "ecx"
        );

        g_APICBase = (uint64)MemoryManager::PhysToKernelPtr((void*)(((uint64)edx << 32) | (eax & 0xFFFFF000)));
        printf("APIC Base: 0x%x\n", g_APICBase);

        IDT::SetISR(ISRNumbers::APICError, ISR_Error);
        IDT::SetIST(ISRNumbers::APICError, 1);

        IDT::SetISR(ISRNumbers::APICSpurious, ISR_Spurious);
        IDT::SetIST(ISRNumbers::APICSpurious, 1);

        IDT::SetISR(ISRNumbers::APICTimer, ISR_Timer);
        IDT::SetIST(ISRNumbers::APICTimer, 1);

        *(volatile uint32*)(g_APICBase + RegSpurious) = 0x100 | ISRNumbers::APICSpurious;
        *(volatile uint32*)(g_APICBase + RegError) = 0x10000 | ISRNumbers::APICError;

        CalibrateTimer();
    }
    void StartTimer(uint8 div, uint32 count, bool repeat)
    {
        switch(div)
        {
        case 1: *(volatile uint32*)(g_APICBase + RegTimerDiv) = 0b1011; break;
        case 2: *(volatile uint32*)(g_APICBase + RegTimerDiv) = 0b0000; break;
        case 4: *(volatile uint32*)(g_APICBase + RegTimerDiv) = 0b0001; break;
        case 8: *(volatile uint32*)(g_APICBase + RegTimerDiv) = 0b0010; break;
        case 16: *(volatile uint32*)(g_APICBase + RegTimerDiv) = 0b0101; break;
        case 32: *(volatile uint32*)(g_APICBase + RegTimerDiv) = 0b1000; break;
        case 64: *(volatile uint32*)(g_APICBase + RegTimerDiv) = 0b1001; break;
        case 128: *(volatile uint32*)(g_APICBase + RegTimerDiv) = 0b1010; break;
        }

        *(volatile uint32*)(g_APICBase + RegTimerMode) = (repeat ? 0x20000 : 0) | ISRNumbers::APICTimer;
        *(volatile uint32*)(g_APICBase + RegTimerInitCount) = count;
    }
    void StartTimer(uint32 ms)
    {
        StartTimer(1, g_TimerTicksPerMS * ms, true);
    }
}