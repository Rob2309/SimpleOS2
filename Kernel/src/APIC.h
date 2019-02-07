#pragma once

#include "types.h"

namespace APIC
{
    void Init();
    void StartTimer(uint8 div, uint32 count, bool repeat);
}