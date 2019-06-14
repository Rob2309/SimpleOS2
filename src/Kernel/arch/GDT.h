#pragma once

#include "types.h"
#include "KernelHeader.h"

namespace GDT
{
    constexpr uint16 KernelCode = 0x08;
    constexpr uint16 KernelData = 0x10;
    constexpr uint16 UserCode   = 0x23;
    constexpr uint16 UserData   = 0x1B;

    void Init(KernelHeader* header);

    void SetIST1(void* addr);
}