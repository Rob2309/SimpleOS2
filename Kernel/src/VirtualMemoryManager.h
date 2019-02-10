#pragma once

#include "KernelHeader.h"

namespace VirtualMemoryManager
{
    void Init(KernelHeader* header);

    void MapPage(uint64 physPage, uint64 virtPage, bool user, bool disableCache = false, bool writeThrough = false);
    void UnmapPage(uint64 virtPage);
}