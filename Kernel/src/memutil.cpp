#include "memutil.h"

void memset(void* dest, int value, uint64 size)
{
    for(uint64 i = 0; i < size; i++)
        ((char*)dest)[i] = value;
}