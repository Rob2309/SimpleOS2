#include "memory/KernelHeap.h"

void* operator new(long unsigned int size)
{
    return KernelHeap::Allocate(size);
}
void* operator new[](long unsigned int size)
{
    return operator new(size);
}

void operator delete(void* block)
{
    KernelHeap::Free(block);
}
void operator delete(void* block, long unsigned int size)
{
    operator delete(block);
}

void operator delete[](void* block, long unsigned int size)
{
    operator delete(block);
}
void operator delete[](void* block)
{
    operator delete(block);
}