#include "KernelHeap.h"

#include "MemoryManager.h"

#include "conio.h"

#include "FreeList.h"
#include "Mutex.h"

namespace KernelHeap {

    constexpr uint64 HeapBase = ((uint64)510 << 39) | 0xFFFF000000000000;

    static Mutex g_Lock;
    static FreeList g_FreeList;
    static uint64 g_HeapPos = HeapBase;

    static void ReserveNew(uint64 size) 
    {
        size = (size + 4095) / 4096;
        void* g = MemoryManager::AllocatePages(size);
        for(int i = 0; i < size; i++)
            MemoryManager::MapKernelPage((char*)g + 4096 * i, (char*)g_HeapPos + 4096 * i);
        g_FreeList.MarkFree((void*)g_HeapPos, size * 4096);
        g_HeapPos += size * 4096;
    }

    void* Allocate(uint64 size)
    {
        size = (size + sizeof(uint64) * 2 + 63) / 64 * 64;

        g_Lock.SpinLock();
        void* g = g_FreeList.FindFree(size);
        if(g == nullptr) {
            ReserveNew(size);
            g = g_FreeList.FindFree(size);
        }
        g_FreeList.MarkUsed(g, size);
        g_Lock.Unlock();

        *(uint64*)g = size;
        return (uint64*)g + 2;
    }
    void Free(void* block)
    {
        if(block == nullptr)
            return;

        uint64* b = (uint64*)block - 2;
        uint64 size = *b;

        g_Lock.SpinLock();
        g_FreeList.MarkFree(b, size);
        g_Lock.Unlock();
    }

}