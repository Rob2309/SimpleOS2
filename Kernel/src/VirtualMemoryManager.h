#pragma once

#include "KernelHeader.h"

namespace VirtualMemoryManager
{
    void Init(KernelHeader* header);

    uint64 MapPage(uint64 physPage, bool disableCache = false);
    void MapPage(uint64 physPage, uint64 virtPage, bool disableCache = false, bool writeThrough = false);
    void UnmapPage(uint64 virtPage);
}