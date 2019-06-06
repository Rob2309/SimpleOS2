#include "memory.h"

void kmemset(void* dest, int value, uint64 size)
{
    for(uint64 i = 0; i < size; i++)
        ((char*)dest)[i] = value;
}

void kmemcpy(void* dest, const void* src, uint64 size)
{
    for(uint64 i = 0; i < size; i++)
        ((char*)dest)[i] = ((char*)src)[i];
}