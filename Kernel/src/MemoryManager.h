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
    void FreeProcessMap(uint64 pml4Entry);
    void SwitchProcessMap(uint64 pml4Entry);

    void* FindProcessMemory(uint64 numPages);
    void MapProcessPage(void* phys, void* virt);
    void UnmapProcessPage(void* virt);

}