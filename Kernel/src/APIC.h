#pragma once

#include "types.h"
#include "IDT.h"

namespace APIC
{
    void Init();
    void StartTimer(uint8 div, uint32 count, bool repeat);
    void StartTimer(uint32 ms);

    typedef void (*TimerEvent)(IDT::Registers* regs);
    void SetTimerEvent(TimerEvent evt);
}