#pragma once

#include "types.h"
#include "KernelHeader.h"

namespace SMP {

    constexpr uint64 MaxCoreCount = 128;

    void GatherInfo(KernelHeader* header);
    void StartCores();

    uint64 GetCoreCount();
    uint64 GetLogicalCoreID();
    uint64 GetApicID(uint64 logicalCore);

}