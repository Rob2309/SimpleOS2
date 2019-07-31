#pragma once

#include "types.h"

namespace Time {

    bool Init();

    uint64 GetTSCTicksPerMilli();
    uint64 GetTSC();

}