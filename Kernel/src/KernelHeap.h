#pragma once

#include "types.h"

namespace KernelHeap {

    void Init();

    void* Allocate(uint64 size);
    void Free(void* block);

}