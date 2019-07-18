#pragma once

#include "types.h"
#include "KernelHeader.h"

namespace GDT
{
    constexpr uint16 KernelCode = 0x08;
    constexpr uint16 KernelData = 0x10;
    constexpr uint16 UserCode   = 0x23;
    constexpr uint16 UserData   = 0x1B;

    /**
     * Initializes the general GDT structure
     **/
    void Init(uint64 coreCount);
    /**
     * Initializes a specific core
     **/
    void InitCore(uint64 coreID);

    /**
     * Sets the stack to be used by this core, when an interrupt is generated
     **/
    void SetIST1(uint64 coreID, void* addr);
}