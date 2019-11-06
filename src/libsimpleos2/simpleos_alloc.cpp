#include "simpleos_alloc.h"

#include "syscall.h"
#include "internal/FreeList.h"

int64 alloc_pages(void* addr, uint64 numPages) {
    return syscall_invoke(syscall_alloc, (uint64)addr, numPages);
}
int64 free_pages(void* addr, uint64 numPages) {
    return syscall_invoke(syscall_free, (uint64)addr, (uint64)numPages);
}

static FreeList g_FreeList;
static uint64 g_HeapPos = 0xFF000000;

static void ReserveNew(uint64 size) {
    size = (size + 4095) / 4096;
    alloc_pages((void*)g_HeapPos, size);
    g_FreeList.MarkFree((void*)g_HeapPos, size * 4096);
    g_HeapPos += size * 4096;
}

void* malloc(int64 size) {
    size = (size + sizeof(uint64) * 2 + 63) / 64 * 64;

    void* g = g_FreeList.FindFree(size);
    if(g == nullptr) {
        ReserveNew(size);
        g = g_FreeList.FindFree(size);
    }
    g_FreeList.MarkUsed(g, size);

    *(uint64*)g = size;
    return (uint64*)g + 2;
}
void free(void* block) {
    if(block == nullptr)
        return;

    uint64* b = (uint64*)block - 2;
    uint64 size = *b;

    g_FreeList.MarkFree(b, size);
}