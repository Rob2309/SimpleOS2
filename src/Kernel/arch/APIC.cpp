#include "APIC.h"

#include "klib/stdio.h"

#include "memory/MemoryManager.h"

#include "port.h"

#include "MSR.h"

namespace APIC
{
    constexpr uint64 RegID = 0x20;

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

    void SignalEOI()
    {
        // By writing a zero to this register, the APIC gets the EOF signal
        *(volatile uint32*)(g_APICBase + RegEOI) = 0;
    }

    // This function will be called when the APIC timer fires an interrupt
    static void ISR_Timer(IDT::Registers* regs)
    {
        if(g_TimerEvent != nullptr)
            g_TimerEvent(regs);
    }
    // This function will be called when the APIC detects any error
    static void ISR_Error(IDT::Registers* regs)
    {
        
    }
    // This function will be called when the APIC fires a Spurious interrupt, should just be ignored
    static void ISR_Spurious(IDT::Registers* regs)
    {
        // Spurious APIC interrupts don't wait for and EOI
    }

    // This function calculates how fast the local APIC timer ticks by comparing it to the PIT timer, which runs at a fixed and known frequency
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
        klog_info_isr("APIC", "APIC Timer runs at %i kHz", g_TimerTicksPerMS);
    }

    void SetTimerEvent(TimerEvent evt)
    {
        g_TimerEvent = evt;
    }

    void Init() {
        uint64 lapicBase = MSR::Read(MSR::RegLAPICBase);    // Physical address of LAPIC memory space

        g_APICBase = (uint64)MemoryManager::PhysToKernelPtr((void*)(lapicBase & 0xFFFFFFFFFFFFF000));
        klog_info_isr("APIC", "APIC Base: 0x%16X", g_APICBase);

        IDT::SetISR(ISRNumbers::APICError, ISR_Error);
        IDT::SetInternalISR(ISRNumbers::APICSpurious, ISR_Spurious);
        IDT::SetISR(ISRNumbers::APICTimer, ISR_Timer);
    }
    void InitBootCore()
    {
        *(volatile uint32*)(g_APICBase + RegSpurious) = 0x100 | ISRNumbers::APICSpurious;
        *(volatile uint32*)(g_APICBase + RegError) = 0x10000 | ISRNumbers::APICError;

        CalibrateTimer();

        // This disables the CPUs caching mechanism for the APIC registers, so that any read/write directly accesses RAM
        // If the cache was enabled, APIC commands might never reach RAM, so the APIC would not receive them
        // I don't know if this is really necessary, as it always worked for me without disabling cache
        MemoryManager::DisableChacheOnLargePage((void*)(g_APICBase & 0xFFFFFFFFFFE00000));
    }
    void InitCore() {
        *(volatile uint32*)(g_APICBase + RegSpurious) = 0x100 | ISRNumbers::APICSpurious;
        *(volatile uint32*)(g_APICBase + RegError) = 0x10000 | ISRNumbers::APICError;
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
    void StartTimerOneshot(uint32 ms) {
        StartTimer(1, g_TimerTicksPerMS * ms, false);
    }

    void SendInitIPI(uint8 coreID) {
        constexpr uint32 cmd = (0b101 << 8) | (1 << 14);
        *(volatile uint32*)(g_APICBase + RegCommandHi) = ((uint32)coreID & 0xF) << 24;
        *(volatile uint32*)(g_APICBase + RegCommandLow) = cmd;
    }

    void SendStartupIPI(uint8 coreID, uint64 startPage) {
        uint32 cmd = (0b110 << 8) | (startPage >> 12);
        *(volatile uint32*)(g_APICBase + RegCommandHi) = ((uint32)coreID & 0xF) << 24;
        *(volatile uint32*)(g_APICBase + RegCommandLow) = cmd;
    }

    void SendIPI(IPITargetMode targetMode, uint8 targetID, uint8 vector) {
        uint32 high = 0;
        uint32 low = 0;

        switch(targetMode) {
        case IPI_TARGET_CORE:
            high = ((uint32)targetID & 0xF) << 24;
            low = vector;
            break;
        case IPI_TARGET_SELF:
            high = 0xFF << 24;
            low = vector | (0b01 << 18);
            break;
        case IPI_TARGET_ALL:
            high = 0xFF << 24;
            low = vector | (0b10 << 18);
            break;
        case IPI_TARGET_ALL_BUT_SELF:
            high = 0xFF << 24;
            low = vector | (0b11 << 18);
            break;
        default:
            return;
        }

        *(volatile uint32*)(g_APICBase + RegCommandHi) = high;
        *(volatile uint32*)(g_APICBase + RegCommandLow) = low;
    }

    uint64 GetID() {
        uint64 res = (*(volatile uint32*)(g_APICBase + RegID)) >> 24;
        return res & 0xF;
    }

}