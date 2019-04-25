#pragma once

#include "types.h"
#include "KernelHeader.h"

namespace MemoryManager {

    void Init(KernelHeader* header);

    /**
     * Allocate physically continuous pages
     **/
    void* AllocatePages(uint64 numPages = 1);
    void FreePages(void* pages, uint64 numPages = 1);

    /**
     * Convert the given physical address to a pointer that can be accessed by the kernel
     **/
    void* PhysToKernelPtr(const void* ptr);
    void* KernelToPhysPtr(const void* ptr);

    uint64 CreateProcessMap();
    uint64 ForkProcessMap();
    void FreeProcessMap(uint64 pml4Entry);

    void SwitchProcessMap(uint64 pml4Entry);

    void MapKernelPage(void* phys, void* virt);
    void UnmapKernelPage(void* virt);
    void MapProcessPage(uint64 pml4Entry, void* phys, void* virt, bool invalidate = true);
    void* MapProcessPage(uint64 pml4Entry, void* virt, bool invalidate);
    void UnmapProcessPage(uint64 pml4Entry, void* virt);

    void* UserToKernelPtr(const void* ptr);

}