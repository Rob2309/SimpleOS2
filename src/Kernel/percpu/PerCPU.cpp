#include "PerCPUInit.h"
#include "PerCPU.h"
#include "memory/MemoryManager.h"
#include "klib/memory.h"

extern "C" int __PER_CPU_START;
extern "C" int __PER_CPU_END;

static char* g_PerCpuBlocks;
static int g_PerCpuBlockSize;

namespace _PerCpuInternal {
    
    void* GetAddress(uint64 coreID, void* val) {
        uint64 offs = (uint64)val - (uint64)&__PER_CPU_START;

        return g_PerCpuBlocks + g_PerCpuBlockSize * coreID + offs;
    }

}

namespace PerCPU {

    void Init(uint64 numCores) {
        g_PerCpuBlockSize = (uint64)&__PER_CPU_END - (uint64)&__PER_CPU_START;
        g_PerCpuBlocks = (char*)MemoryManager::PhysToKernelPtr(MemoryManager::EarlyAllocatePages(NUM_PAGES(g_PerCpuBlockSize * numCores)));
        
        for(int i = 0; i < numCores; i++)
            kmemcpy(g_PerCpuBlocks + i * g_PerCpuBlockSize, &__PER_CPU_START, g_PerCpuBlockSize);
    }

}