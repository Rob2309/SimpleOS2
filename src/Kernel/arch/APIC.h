#pragma once

#include "types.h"
#include "interrupts/IDT.h"

namespace APIC
{
    /**
     * Initializes the APIC, has to be called before using any APIC function
     **/
    void Init();
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

    typedef void (*TimerEvent)(IDT::Registers* regs);
    /**
     * Sets the function to be called when the timer fires
     **/
    void SetTimerEvent(TimerEvent evt);

    void SendInitIPI(uint8 coreID);
    void SendStartupIPI(uint8 coreID, uint64 startPage);

    uint64 GetID();
}