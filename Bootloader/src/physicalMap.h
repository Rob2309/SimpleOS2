#pragma once

#include "KernelHeader.h"

namespace PhysicalMap {

    struct PhysMapInfo
    {
        PhysicalMapSegment* map;
        uint64 size;
        uint64 numSegments;
    };
    
    PhysMapInfo Build();

}