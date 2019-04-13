#pragma once

#include "types.h"
#include "KernelHeader.h"

namespace MemoryManager {

    void Init(KernelHeader* header);

    void* AllocatePages(uint64 numPages = 1);
    void FreePages(void* pages, uint64 numPages = 1);

    void* PhysToKernelPtr(const void* ptr);
    void* KernelToPhysPtr(const void* ptr);

    uint64 CreateProcessMap();
    uint64 ForkProcessMap();
    void FreeProcessMap(uint64 pml4Entry);

    void SwitchProcessMap(uint64 pml4Entry);

    void MapKernelPage(void* phys, void* virt);
    void UnmapKernelPage(void* virt);
    void MapProcessPage(uint64 pml4Entry, void* phys, void* virt, bool invalidate = true);
    void UnmapProcessPage(uint64 pml4Entry, void* virt, bool invalidate = true);

    void* UserToKernelPtr(const void* ptr);

}