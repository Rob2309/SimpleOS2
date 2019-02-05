#pragma once

#include "types.h"
#include "KernelHeader.h"

namespace PhysicalMemoryManager {

    void Init(KernelHeader* header);

    void* AllocatePage();
    void FreePage(void* page);

    void* AllocatePages(uint64 numPages);
    void FreePages(void* pages, uint64 numPages);

    void* AllocateCleanPage();

    

}