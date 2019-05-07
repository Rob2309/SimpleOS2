#pragma once

#include "types.h"

namespace KernelHeap {

    /**
     * Allocates [size] bytes on the kernel heap. !Can only be used while Interrupts are enabled!
     * The advantage over MemoryManager::AllocatePages is that the physical memory does not have to be contiguous.
     **/
    void* Allocate(uint64 size);
    /**
     * Frees memory allocated with Allocate
     **/
    void Free(void* block);

}