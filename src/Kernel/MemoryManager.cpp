#include "MemoryManager.h"

#include "conio.h"
#include "memutil.h"
#include "FreeList.h"

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

namespace MemoryManager {

    static FreeList g_FreeList;

    static uint64 g_HighMemBase;

    static uint64* g_PML4;

    void Dump()
    {
        for(auto seg : g_FreeList) {
            printf("0x%x - 0x%x\n", seg.base, seg.base + seg.size - 1);
        }
    }

    void Init(KernelHeader* header)
    {
        g_FreeList = FreeList(header->physMapStart);
        g_HighMemBase = header->highMemoryBase;
        g_PML4 = header->pageBuffer;

        // delete lower half mapping
        g_PML4[0] = 0;

        uint64 availableMemory = 0;
        for(auto a : g_FreeList) {
            availableMemory += a.size;
        }

        uint64* heapPML3 = (uint64*)PhysToKernelPtr(AllocatePages(1));
        for(int i = 0; i < 512; i++)
            heapPML3[i] = 0;
        g_PML4[510] = PML_SET_ADDR((uint64)KernelToPhysPtr(heapPML3)) | PML_SET_P(1) | PML_SET_RW(1);

        printf("Available memory: %i MB\n", availableMemory / 1024 / 1024);
        printf("Memory manager initialized\n");
    }

    void* AllocatePages(uint64 numPages)
    {
        void* p = g_FreeList.FindFree(numPages * 4096);
        if(p == nullptr) {
            return nullptr;
        }

        g_FreeList.MarkUsed(p, numPages * 4096);

        return KernelToPhysPtr(p);
    }

    void FreePages(void* pages, uint64 numPages)
    {
        g_FreeList.MarkFree(PhysToKernelPtr(pages), numPages * 4096);
    }

    void* PhysToKernelPtr(void* ptr)
    {
        return (char*)ptr + g_HighMemBase;
    }

    void* KernelToPhysPtr(void* ptr)
    {
        return (char*)ptr - g_HighMemBase;
    }

    static void FreeProcessPML1(uint64* pml1)
    {
        for(int i = 0; i < 512; i++) {
            uint64 pml1Entry = pml1[i];
            if(PML_GET_P(pml1Entry)) {
                MemoryManager::FreePages((void*)PML_GET_ADDR(pml1Entry));
            }
        }
        FreePages(KernelToPhysPtr(pml1));
    }
    static void FreeProcessPML2(uint64* pml2)
    {
        for(int i = 0; i < 512; i++) {
            uint64 pml2Entry = pml2[i];
            if(PML_GET_P(pml2Entry))
                FreeProcessPML1((uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml2Entry)));
        }
        FreePages(KernelToPhysPtr(pml2));
    }
    static void FreeProcessPML3(uint64* pml3)
    {
        for(int i = 0; i < 512; i++) {
            uint64 pml3Entry = pml3[i];
            if(PML_GET_P(pml3Entry))
                FreeProcessPML2((uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml3Entry)));
        }
        FreePages(KernelToPhysPtr(pml3));
    }

    uint64 CreateProcessMap()
    {
        uint64* pml3 = (uint64*)PhysToKernelPtr(AllocatePages());
        for(int i = 0; i < 512; i++)
            pml3[i] = 0;

        uint64 pml4Entry = PML_SET_ADDR((uint64)KernelToPhysPtr(pml3)) | PML_SET_P(1) | PML_SET_US(1) | PML_SET_RW(1);
        return pml4Entry;
    }
    uint64 ForkProcessMap()
    {
        uint64 newPML4Entry = CreateProcessMap();

        uint64* pml3 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(g_PML4[0]));

        for(uint64 i = 0; i < 512; i++) {
            uint64 pml3Entry = pml3[i];
            if(PML_GET_P(pml3Entry)) {
                uint64* pml2 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml3Entry));
                for(uint64 j = 0; j < 512; j++) {
                    uint64 pml2Entry = pml2[j];
                    if(PML_GET_P(pml2Entry)) {
                        uint64* pml1 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml2Entry));
                        for(uint64 k = 0; k < 512; k++) {
                            uint64 pml1Entry = pml1[k];
                            if(PML_GET_P(pml1Entry)) {
                                char* virt = (char*)((k << 12) | (j << 21) | (i << 30));

                                uint64* dest = (uint64*)PhysToKernelPtr(AllocatePages());
                                uint64* src = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml1Entry));
                                memcpy(dest, src, 4096);
                                MapProcessPage(newPML4Entry, KernelToPhysPtr(dest), virt, false);
                            }
                        }
                    }
                }
            }
        }

        return newPML4Entry;
    }

    void FreeProcessMap(uint64 pml4Entry)
    {
        uint64* pml3 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml4Entry));
        FreeProcessPML3(pml3);
    }
    void SwitchProcessMap(uint64 pml4Entry)
    {
        g_PML4[0] = pml4Entry;

        __asm__ __volatile__ (
            "movq %0, %%cr3"
            : : "r"(KernelToPhysPtr(g_PML4))
        );
    }

    void MapKernelPage(void* phys, void* virt)
    {
        uint64 pml4Index = GET_PML4_INDEX((uint64)virt);
        uint64 pml3Index = GET_PML3_INDEX((uint64)virt);
        uint64 pml2Index = GET_PML2_INDEX((uint64)virt);
        uint64 pml1Index = GET_PML1_INDEX((uint64)virt);

        uint64 pml4Entry = g_PML4[pml4Index];
        uint64* pml3 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml4Entry));

        uint64 pml3Entry = pml3[pml3Index];
        uint64* pml2;
        if(!PML_GET_P(pml3Entry)) {
            pml2 = (uint64*)PhysToKernelPtr(AllocatePages());
            for(int i = 0; i < 512; i++)
                pml2[i] = 0;
            pml3[pml3Index] = PML_SET_ADDR((uint64)KernelToPhysPtr(pml2)) | PML_SET_P(1) | PML_SET_RW(1);
        } else {
            pml2 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml3Entry));
        }

        uint64 pml2Entry = pml2[pml2Index];
        uint64* pml1;
        if(!PML_GET_P(pml2Entry)) {
            pml1 = (uint64*)PhysToKernelPtr(AllocatePages());
            for(int i = 0; i < 512; i++)
                pml1[i] = 0;
            pml2[pml2Index] = PML_SET_ADDR((uint64)KernelToPhysPtr(pml1)) | PML_SET_P(1) | PML_SET_RW(1);
        } else {
            pml1 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml2Entry));
        }

        pml1[pml1Index] = PML_SET_ADDR((uint64)phys) | PML_SET_P(1) | PML_SET_RW(1);

        __asm__ __volatile__ (
            "invlpg (%0)"
            : : "r"(virt)
        );
    }
    void UnmapKernelPage(void* virt)
    {
        uint64 pml4Index = GET_PML4_INDEX((uint64)virt);
        uint64 pml3Index = GET_PML3_INDEX((uint64)virt);
        uint64 pml2Index = GET_PML2_INDEX((uint64)virt);
        uint64 pml1Index = GET_PML1_INDEX((uint64)virt);

        uint64 pml4Entry = g_PML4[pml4Entry];
        uint64* pml3 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml4Entry));

        uint64 pml3Entry = pml3[pml3Index];
        uint64* pml2 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml3Entry));

        uint64 pml2Entry = pml2[pml2Index];
        uint64* pml1 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml2Entry));

        pml1[pml1Index] = 0;

        __asm__ __volatile__ (
            "invlpg (%0)"
            : : "r"(virt)
        );
    }

    void MapProcessPage(uint64 pml4Entry, void* phys, void* virt, bool invalidate)
    {
        uint64 pml3Index = GET_PML3_INDEX((uint64)virt);
        uint64 pml2Index = GET_PML2_INDEX((uint64)virt);
        uint64 pml1Index = GET_PML1_INDEX((uint64)virt);

        uint64* pml3 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml4Entry));

        uint64 pml3Entry = pml3[pml3Index];
        uint64* pml2;
        if(!PML_GET_P(pml3Entry)) {
            pml2 = (uint64*)PhysToKernelPtr(AllocatePages());
            for(int i = 0; i < 512; i++)
                pml2[i] = 0;
            pml3[pml3Index] = PML_SET_ADDR((uint64)KernelToPhysPtr(pml2)) | PML_SET_P(1) | PML_SET_RW(1) | PML_SET_US(1);
        } else {
            pml2 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml3Entry));
        }

        uint64 pml2Entry = pml2[pml2Index];
        uint64* pml1;
        if(!PML_GET_P(pml2Entry)) {
            pml1 = (uint64*)PhysToKernelPtr(AllocatePages());
            for(int i = 0; i < 512; i++)
                pml1[i] = 0;
            pml2[pml2Index] = PML_SET_ADDR((uint64)KernelToPhysPtr(pml1)) | PML_SET_P(1) | PML_SET_RW(1) | PML_SET_US(1);
        } else {
            pml1 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml2Entry));
        }

        pml1[pml1Index] = PML_SET_ADDR((uint64)phys) | PML_SET_P(1) | PML_SET_RW(1) | PML_SET_US(1);

        if(invalidate) {
            __asm__ __volatile__ (
                "invlpg (%0)"
                : : "r"(virt)
            );
        }
    }
    void UnmapProcessPage(uint64 pml4Entry, void* virt, bool invalidate)
    {
        uint64 pml3Index = GET_PML3_INDEX((uint64)virt);
        uint64 pml2Index = GET_PML2_INDEX((uint64)virt);
        uint64 pml1Index = GET_PML1_INDEX((uint64)virt);

        uint64* pml3 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml4Entry));

        uint64 pml3Entry = pml3[pml3Index];
        uint64* pml2 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml3Entry));

        uint64 pml2Entry = pml2[pml2Index];
        uint64* pml1 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml2Entry));

        pml1[pml1Index] = 0;

        if(invalidate) {
            __asm__ __volatile__ (
                "invlpg (%0)"
                : : "r"(virt)
            );
        }
    }

}