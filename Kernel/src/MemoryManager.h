#pragma once

#include "types.h"
#include "KernelHeader.h"

namespace MemoryManager {

    void Init(KernelHeader* header);

    void* AllocatePages(uint64 numPages = 1);
    void FreePages(void* pages, uint64 numPages = 1);

    void* PhysToKernelPtr(void* ptr);
    void* KernelToPhysPtr(void* ptr);

    uint64 CreateProcessMap();
    uint64 ForkProcessMap();
    void FreeProcessMap(uint64 pml4Entry);

    void SwitchProcessMap(uint64 pml4Entry);

    void MapProcessPage(uint64 pml4Entry, void* phys, void* virt, bool invalidate = true);
    void UnmapProcessPage(uint64 pml4Entry, void* virt, bool invalidate = true);

}