#pragma once

#include "types.h"
#include "KernelHeader.h"

namespace MemoryManager {

    void Init(KernelHeader* header);

    void* AllocatePages(uint64 numPages = 1);
    void FreePages(void* pages, uint64 numPages = 1);

    void* PhysToKernelPtr(void* ptr);
    void* KernelToPhysPtr(void* ptr);

}