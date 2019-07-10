#pragma once

#include "types.h"
#include "KernelHeader.h"

constexpr uint64 NUM_PAGES(uint64 size) {
    return (size + 4095) / 4096;
}

namespace MemoryManager {

    void Init(KernelHeader* header);
    void InitCore(uint64 coreID);

    void DisableChacheOnLargePage(void* virt);

    void* EarlyAllocatePages(uint64 numPages);
    void EarlyFreePages(void* pages, uint64 numPages);

    /**
     * Allocate physically continuous pages.
     * Can only be called from a thread.
     **/
    void* AllocatePages(uint64 numPages = 1);
    /**
     * Free pages Allocated with AllocatePages.
     * Can only be called from a thread.
     */
    void FreePages(void* pages, uint64 numPages = 1);

    void InvalidatePage(void* page);

    /**
     * Convert the given physical address to a pointer that can be accessed by the kernel
     **/
    void* PhysToKernelPtr(const void* ptr);
    /**
     * Convert the given Kernel pointer to the physical address it represents
     **/
    void* KernelToPhysPtr(const void* ptr);

    /**
     * Create a new Paging structure to be used by a user process
     **/
    uint64 CreateProcessMap();
    /**
     * Clone the User Memory space of the current process
     **/
    uint64 ForkProcessMap();
    /**
     * Clean up the User Memory space associated with the given pml4Entry
     **/
    void FreeProcessMap(uint64 pml4Entry);

    /**
     * Switch to a given User Memory Space
     **/
    void SwitchProcessMap(uint64 pml4Entry);

    /**
     * Map a physical page to be accessible by the kernel only
     **/
    void MapKernelPage(void* phys, void* virt);
    /**
     * Map a physical page into the given User Memory Space
     **/
    void MapProcessPage(uint64 pml4Entry, void* phys, void* virt, bool invalidate = true);
    /**
     * Map any available physical page to the given virtual page. If the virtual page is already mapped to any physical page, do nothing
     **/
    void* MapProcessPage(uint64 pml4Entry, void* virt, bool invalidate);
    void MapProcessPage(void* virt);
    /**
     * Remove a page from the given User Memory Space
     **/
    void UnmapProcessPage(uint64 pml4Entry, void* virt);
    void UnmapProcessPage(void* virt);

    /**
     * Return a kernel-usable pointer to the physical page mapped to the given virtual address. If it is not mapped, return nullptr
     **/
    void* UserToKernelPtr(const void* ptr);

    bool IsUserPtr(const void* ptr);

}