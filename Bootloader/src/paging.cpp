#include "paging.h"

#include "allocator.h"

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
    static uint64* g_PML4;
    static uint64* g_HighPML3;

    void Init()
    {
        __asm__ __volatile__ (
            "movq %%cr3, %0"
            : "=r"(g_PML4)
        );

        g_HighPML3 = (uint64*)Allocate(4096);
        for(int i = 0; i < 512; i++)
            g_HighPML3[i] = 0;

        g_PML4[511] = PML_SET_ADDR((uint64)g_HighPML3) | PML_SET_RW(1) | PML_SET_P(1);
    }

    uint64 MapHighPage(uint64 physPage)
    {
        uint64 pml3Index = GET_PML3_INDEX(physPage);
        uint64 pml2Index = GET_PML2_INDEX(physPage);
        uint64 pml1Index = GET_PML1_INDEX(physPage);

        uint64 pml3Entry = g_HighPML3[pml3Index];
        uint64* pml2 = (uint64*)PML_GET_ADDR(pml3Entry);
        if(!PML_GET_P(pml3Entry)) {
            pml2 = (uint64*)Allocate(4096);
            g_HighPML3[pml3Index] = PML_SET_ADDR((uint64)pml2) | PML_SET_RW(1) | PML_SET_P(1);
        }

        uint64 pml2Entry = pml2[pml2Index];
        uint64* pml1 = (uint64*)PML_GET_ADDR(pml2Entry);
        if(!PML_GET_P(pml2Entry)) {
            pml1 = (uint64*)Allocate(4096);
            pml2[pml2Index] = PML_SET_ADDR((uint64)pml1) | PML_SET_RW(1) | PML_SET_P(1);
        }

        pml1[pml1Index] = PML_SET_ADDR((uint64)physPage) | PML_SET_RW(1) | PML_SET_P(1);
        
        uint64 virtualAddress = 0xFFFFFF8000000000 | (physPage & 0x7FFFFFF000);       
        __asm__ __volatile__ (
            "invlpg (%0)"
            : : "r"(virtualAddress)
        );
        
        return virtualAddress;
    }

    uint64 MapHighPages(uint64 physPage, uint64 numPages)
    {
        for(uint64 i = 0; i < numPages; i++)
            MapHighPage(physPage + i * 4096);

        uint64 virtualAddress = 0xFFFFFF8000000000 | (physPage & 0x7FFFFFF000);
        return virtualAddress;
    }
}