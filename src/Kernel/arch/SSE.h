#pragma once

#include "types.h"

namespace SSE {
    bool Init();

    uint64 GetFPUBlockSize();
    void SaveFPUBlock(char* buffer);
    void RestoreFPUBlock(char* buffer);
}