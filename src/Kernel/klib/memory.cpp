#include "memory.h"

void kmemset(void* dest, int value, uint64 size)
{
    for(uint64 i = 0; i < size; i++)
        ((char*)dest)[i] = value;
}

void kmemcpy(void* dest, const void* src, uint64 size) {
    char* d = (char*)dest;
    char* s = (char*)src;

    uint64 numq = size >> 3;
    kmemcpyq(dest, src, numq);

    uint64 rem = size & 0x7;
    kmemcpyb(d + (numq << 3), s + (numq << 3), rem);
}

void kmemcpyb(void* dest, const void* src, uint64 size)
{
    __asm__ __volatile__ (
        "rep movsb"
        : "+S"(src), "+D"(dest), "+c"(size)
    );
}
void kmemcpyd(void* dest, const void* src, uint64 count) {
    __asm__ __volatile__ (
        "rep movsd"
        : "+S"(src), "+D"(dest), "+c"(count)
    );
}
void kmemcpyq(void* dest, const void* src, uint64 count) {
    __asm__ __volatile__ (
        "rep movsq"
        : "+S"(src), "+D"(dest), "+c"(count)
    );
}