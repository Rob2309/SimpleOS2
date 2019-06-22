#pragma once

#include "types.h"
#include "interrupts/IDT.h"

namespace APIC
{
    /**
     * Initializes the APIC, has to be called before using any APIC function
     **/
    void Init();
    void InitBootCore();
    void InitCore();
    /**
     * Starts the Local APIC Timer with the specified duration.
     * @param div: the number to divide the clock by
     * @param count: the number of timer ticks to wait
     * @param repeat: if true, the timer will fire until it is turned off, if false, the timer will only fire once
     **/
    void StartTimer(uint8 div, uint32 count, bool repeat);
    /**
     * Starts the timer on repeat mode with the specified interval (in milliseconds)
     **/
    void StartTimer(uint32 ms);

    void SignalEOI();

    typedef void (*TimerEvent)(IDT::Registers* regs);
    /**
     * Sets the function to be called when the timer fires
     **/
    void SetTimerEvent(TimerEvent evt);

    void SendInitIPI(uint8 coreID);
    void SendStartupIPI(uint8 coreID, uint64 startPage);

    enum IPITargetMode {
        IPI_TARGET_CORE = 0,
        IPI_TARGET_SELF = 1,
        IPI_TARGET_ALL = 2,
        IPI_TARGET_ALL_BUT_SELF = 3,
    };
    void SendIPI(IPITargetMode targetMode, uint8 targetID, uint8 vector);

    uint64 GetID();
}