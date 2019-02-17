#include "MemoryManager.h"

#include "conio.h"
#include "memutil.h"

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

    static PhysicalMapSegment* g_PhysMap = nullptr;
    static uint64 g_PhysMapPages = 0;
    static uint64 g_PhysMapSegments;

    static uint64 g_HighMemBase;

    static uint64* g_PML4;

    void* GetPhysicalMapPointer()
    {
        return g_PhysMap;
    }
    uint64 GetPhysicalMapPageCount()
    {
        return g_PhysMapPages;
    }

    static PhysicalMapSegment* FindSegment(void* physPage) {
        PhysicalMapSegment* physMap = g_PhysMap;

        for(uint64 s = 0; s < g_PhysMapSegments; s++) {
            if(physMap->base <= (uint64)physPage && (physMap->base + physMap->numPages * 4096 - 1) >= (uint64)physPage) {
                return physMap;
            } else {
                physMap = (PhysicalMapSegment*)((char*)physMap + sizeof(PhysicalMapSegment) + (physMap->numPages + 7) / 8);
            }
        }

        return nullptr;
    }

    static void MarkUsed(PhysicalMapSegment* segment, uint64 pageIndex, uint64 numPages) {
        for(uint64 i = 0; i < numPages; i++) {
            uint64 byte = (pageIndex + i) / 8;
            uint64 bit = (pageIndex + i) % 8;

            segment->map[byte] |= (1 << bit);
        }
    }
    static void MarkUsed(void* physPage, uint64 numPages) {
        PhysicalMapSegment* segment = FindSegment(physPage);
        uint64 index = ((uint64)physPage - segment->base) / 4096;

        MarkUsed(segment, index, numPages);
    }

    static void MarkFree(PhysicalMapSegment* segment, uint64 pageIndex, uint64 numPages) {
        for(uint64 i = 0; i < numPages; i++) {
            uint64 byte = (pageIndex + i) / 8;
            uint64 bit = (pageIndex + i) % 8;

            segment->map[byte] &= ~(1 << bit);
        }
    }
    static void MarkFree(void* physPage, uint64 numPages) {
        PhysicalMapSegment* segment = FindSegment(physPage);
        uint64 index = ((uint64)physPage - segment->base) / 4096;

        MarkFree(segment, index, numPages);
    }

    void Init(KernelHeader* header)
    {
        g_PhysMap = header->physMap;
        g_PhysMapPages = header->physMapPages;
        g_PhysMapSegments = header->physMapSegments;
        g_HighMemBase = header->highMemoryBase;
        g_PML4 = header->pageBuffer;

        // delete lower half mapping
        g_PML4[0] = 0;

        printf("Physical memory map at 0x%x\n", g_PhysMap);
        printf("Size needed for physical memory map: %i Bytes\n", g_PhysMapPages * 4096);

        uint64 availableMemory = 0;

        PhysicalMapSegment* physMap = g_PhysMap;
        for(uint64 s = 0; s < g_PhysMapSegments; s++) {
            availableMemory += physMap->numPages * 4096;
            physMap = (PhysicalMapSegment*)((char*)physMap + sizeof(PhysicalMapSegment) + (physMap->numPages + 7) / 8);
        }

        printf("Available memory: %i MB\n", availableMemory / 1024 / 1024);

        MarkUsed(KernelToPhysPtr(header), (sizeof(KernelHeader) + 4095) / 4096);
        MarkUsed(KernelToPhysPtr(header->kernelImage.buffer), header->kernelImage.numPages);
        MarkUsed(KernelToPhysPtr(header->fontImage.buffer), header->fontImage.numPages);
        MarkUsed(KernelToPhysPtr(header->ramdiskImage.buffer), header->ramdiskImage.numPages);
        MarkUsed(KernelToPhysPtr(header->physMap), header->physMapPages);
        MarkUsed(KernelToPhysPtr(header->stack), header->stackPages);
        MarkUsed(KernelToPhysPtr(header->pageBuffer), header->pageBufferPages);

        printf("Physical memory map initialized\n");
    }

    void* AllocatePages(uint64 numPages)
    {
        PhysicalMapSegment* seg = g_PhysMap;
        for(int s = 0; s < g_PhysMapSegments; s++) {
            uint64 numFree = 0;
            for(uint64 p = 0; p < seg->numPages; p++) {
                uint64 byte = p / 8;
                uint64 bit = p % 8;

                if((seg->map[byte] & (1 << bit)) == 0) {
                    numFree++;
                    if(numFree == numPages) {
                        uint64 basePage = p - numPages + 1;
                        MarkUsed(seg, basePage, numPages);
                        return (void*)(seg->base + basePage * 4096);
                    }
                } else {
                    numFree = 0;
                }
            }

            seg = (PhysicalMapSegment*)((char*)seg + sizeof(PhysicalMapSegment) + (seg->numPages + 7) / 8);
        }

        return nullptr;
    }
    void FreePages(void* pages, uint64 numPages)
    {
        MarkFree(pages, numPages);
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