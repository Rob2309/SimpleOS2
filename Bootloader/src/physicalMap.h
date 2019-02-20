#pragma once

#include "KernelHeader.h"
#include "efiutil.h"

namespace PhysicalMap {
    
    PhysicalMapSegment* Build(EfiMemoryMap memMap);

}