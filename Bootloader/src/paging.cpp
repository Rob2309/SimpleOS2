#include "paging.h"

#include "allocator.h"
#include "efiutil.h"

#define PML_GET_NX(entry)           ((entry) & 0x8000000000000000)
#define PML_GET_ADDR(entry)         ((entry) & 0x000FFFFFFFFFF000)

#define PML_SET_NX                  0x8000000000000000
#define PML_SET_ADDR(addr)          (addr)

#define PML_GET_A(entry)            ((entry) & 0x20)
#define PML_GET_PCD(entry)          ((entry) & 0x10)
#define PML_GET_PWT(entry)          ((entry) & 0x8)
#define PML_GET_US(entry)           ((entry) & 0x4)
#define PML_GET_RW(entry)           ((entry) & 0x2)
#define PML_GET_P(entry)            ((entry) & 0x1)

#define PML_SET_A(a)                ((a) ? 0x20 : 0)
#define PML_SET_PCD(a)              ((a) ? 0x10 : 0)
#define PML_SET_PWT(a)              ((a) ? 0x8 : 0)
#define PML_SET_US(a)               ((a) ? 0x4 : 0)
#define PML_SET_RW(a)               ((a) ? 0x2 : 0)
#define PML_SET_P(a)                ((a) ? 0x1 : 0)

#define PML1_GET_G(entry)           ((entry) & 0x100)
#define PML1_GET_PAT(entry)         ((entry) & 0x80)
#define PML1_GET_D(entry)           ((entry) & 0x40)

#define PML1_SET_G(a)               ((a) ? 0x100 : 0)
#define PML1_SET_PAT(a)             ((a) ? 0x80 : 0)
#define PML1_SET_D(a)               ((a) ? 0x40 : 0)

#define GET_PML1_INDEX(addr)        (((addr) >> (12 + 0 * 9)) & 0x1FF)
#define GET_PML2_INDEX(addr)        (((addr) >> (12 + 1 * 9)) & 0x1FF)
#define GET_PML3_INDEX(addr)        (((addr) >> (12 + 2 * 9)) & 0x1FF)
#define GET_PML4_INDEX(addr)        (((addr) >> (12 + 3 * 9)) & 0x1FF)

namespace Paging
{
    static uint64 g_HighMemBase = 0;

    void Init(KernelHeader* header)
    {
        uint64* pagingBuffer = (uint64*)Allocate(4096 + 4096 + 512 * 4096, (EFI_MEMORY_TYPE)0x80000001);

        for(int i = 0; i < 512; i++)
            pagingBuffer[i] = 0;
        pagingBuffer[0] = pagingBuffer[511] = PML_SET_ADDR((uint64)&pagingBuffer[512]) | PML_SET_P(1) | PML_SET_RW(1);

        for(int i = 0; i < 512; i++)
            pagingBuffer[512 + i] = PML_SET_ADDR((uint64)&pagingBuffer[1024 + 512 * i]) | PML_SET_P(1) | PML_SET_RW(1);

        for(uint64 i = 0; i < 512 * 512; i++)
            pagingBuffer[1024 + i] = PML_SET_ADDR(i << 21) | PML_SET_P(1) | PML_SET_RW(1) | PML1_SET_PAT(1);

        __asm__ __volatile__ (
            "movq %0, %%cr3"
            : : "r"(pagingBuffer)
        );

        g_HighMemBase = ((uint64)511 << 39) | 0xFFFF000000000000;
        
        header->pageBuffer = (uint64*)ConvertPtr(pagingBuffer);
        header->pageBufferPages = 1 + 1 + 512;
        header->highMemoryBase = g_HighMemBase;
    }

    void* ConvertPtr(void* ptr)
    {
        return (void*)((uint64)ptr + g_HighMemBase);
    }

}