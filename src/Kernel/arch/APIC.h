#pragma once

#include "types.h"
#include "interrupts/IDT.h"

namespace APIC
{
    /**
     * Initializes the APIC, has to be called before using any APIC function
     **/
    void Init();
    /**
     * Initializes the boot core APIC
     **/
    void InitBootCore();
    /**
     * Initializes any other core's APIC
     **/
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
    /**
     * Starts the timer in one shot mode with the specified interval (in milliseconds)
     **/
    void StartTimerOneshot(uint32 ms);

    /**
     * Signals and End of Interrupt to the APIC, has to be called as soon as possible in any interrupt issued by the APIC
     **/
    void SignalEOI();

    typedef void (*TimerEvent)(IDT::Registers* regs);
    /**
     * Sets the function to be called when the timer fires
     **/
    void SetTimerEvent(TimerEvent evt);

    /**
     * Sends an INIT IPI to the core with the given ID
     **/
    void SendInitIPI(uint8 coreID);
    /**
     * Sends the Startup IPI to the core with the given ID.
     * The core will start executing code in real mode at the given memory page
     **/
    void SendStartupIPI(uint8 coreID, uint64 startPage);

    enum IPITargetMode {
        IPI_TARGET_CORE = 0,
        IPI_TARGET_SELF = 1,
        IPI_TARGET_ALL = 2,
        IPI_TARGET_ALL_BUT_SELF = 3,
    };
    /**
     * Sends an inter-processor interrupt to another core's APIC.
     * When targetMode is IPI_TARGET_CORE, the interrupt is sent to exactly one APIC, specified by targetID.
     * When targetMode is IPI_TARGET_SELF, the interrupt is sent to the calling core, targetID is ignored.
     * When targetMode is IPI_TARGET_ALL, the interrupt is sent to every available APIC, targetID is ignored.
     * When targetMode is IPI_TARGET_ALL_BUT_SELF, the interrupt is sent to every available APIC except the calling one, targetID is ignored.
     * vector specifies the interrupt number to invoke on the target(s)
     **/
    void SendIPI(IPITargetMode targetMode, uint8 targetID, uint8 vector);

    /**
     * Gets the calling cores local APIC ID
     **/
    uint64 GetID();
}