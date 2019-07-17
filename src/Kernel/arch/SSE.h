#pragma once

#include "types.h"

namespace SSE {
    bool Init();
    void InitCore();

    uint64 GetFPUBlockSize();
    void SaveFPUBlock(char* buffer);
    void RestoreFPUBlock(char* buffer);
    void InitFPUBlock(char* buffer);

}