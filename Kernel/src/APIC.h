#pragma once

#include "types.h"

namespace APIC
{
    void Init();
    void StartTimer(uint8 div, uint32 count, bool repeat);
    void StartTimer(uint32 ms);

    typedef void (*TimerEvent)();
    void SetTimerEvent(TimerEvent evt);
}