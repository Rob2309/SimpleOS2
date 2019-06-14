#include "APIC.h"

#include "klib/stdio.h"

#include "memory/MemoryManager.h"

#include "port.h"

#include "MSR.h"

namespace APIC
{
    constexpr uint64 RegSpurious = 0xF0;
    constexpr uint64 RegError = 0x370;
    constexpr uint64 RegEOI = 0xB0;

    constexpr uint64 RegTimerDiv = 0x3E0;
    constexpr uint64 RegTimerMode = 0x320;
    constexpr uint64 RegTimerCurrentCount = 0x390;
    constexpr uint64 RegTimerInitCount = 0x380;

    constexpr uint64 RegCommandLow = 0x300;
    constexpr uint64 RegCommandHi = 0x310;

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
        // Spurious APIC interrupts don't wait for and EOI
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
        klog_info("APIC", "APIC Timer runs at %i kHz", g_TimerTicksPerMS);
    }

    void SetTimerEvent(TimerEvent evt)
    {
        g_TimerEvent = evt;
    }

    void Init()
    {
        uint64 lapicBase = MSR::Read(MSR::RegLAPICBase);    // Physical address of LAPIC memory space

        g_APICBase = (uint64)MemoryManager::PhysToKernelPtr((void*)(lapicBase & 0xFFFFFFFFFFFFF000));
        klog_info("APIC", "APIC Base: 0x%x", g_APICBase);

        IDT::SetISR(ISRNumbers::APICError, ISR_Error);
        IDT::SetISR(ISRNumbers::APICSpurious, ISR_Spurious);
        IDT::SetISR(ISRNumbers::APICTimer, ISR_Timer);

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

    void SendInitIPI(uint8 coreID) {
        constexpr uint32 cmd = (0b101 << 8) | (1 << 14);
        *(volatile uint32*)(g_APICBase + RegCommandHi) = (uint32)coreID << 24;
        *(volatile uint32*)(g_APICBase + RegCommandLow) = cmd;
    }

    void SendStartupIPI(uint8 coreID, uint64 startPage) {
        uint32 cmd = (0b110 << 8) | (startPage >> 12);
        *(volatile uint32*)(g_APICBase + RegCommandHi) = (uint32)coreID << 24;
        *(volatile uint32*)(g_APICBase + RegCommandLow) = cmd;
    }

}