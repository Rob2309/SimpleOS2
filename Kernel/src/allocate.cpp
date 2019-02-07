#include "allocate.h"

#include "PhysicalMemoryManager.h"
#include "VirtualMemoryManager.h"

void* AllocatePages(uint64 numPages)
{
    char* res = (char*)PhysicalMemoryManager::AllocatePages(numPages);
    for(uint64 i = 0; i < numPages; i++)
        VirtualMemoryManager::MapPage((uint64)(res + i * 4096), (uint64)(res + i * 4096));
    return res;
}