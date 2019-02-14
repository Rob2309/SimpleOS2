#include "MemoryManager.h"

#include "conio.h"

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

    static PhysicalMapSegment* FindSegment(void* page) {
        page = KernelToPhysPtr(page);

        PhysicalMapSegment* physMap = g_PhysMap;

        for(uint64 s = 0; s < g_PhysMapSegments; s++) {
            if(physMap->base <= (uint64)page && (physMap->base + physMap->numPages * 4096 - 1) >= (uint64)page) {
                return physMap;
            } else {
                physMap = (PhysicalMapSegment*)((char*)physMap + sizeof(PhysicalMapSegment) + (physMap->numPages + 7) / 8);
            }
        }

        return nullptr;
    }

    static void MarkUsed(void* page, uint64 numPages) {
        PhysicalMapSegment* segment = FindSegment(page);

        page = KernelToPhysPtr(page);
        uint64 index = ((uint64)page - segment->base) / 4096;

        for(uint64 i = 0; i < numPages; i++) {
            uint64 byte = (index + i) / 8;
            uint64 bit = (index + i) % 8;

            segment->map[byte] |= (1 << bit);
        }
    }

    static void MarkFree(void* page, uint64 numPages) {
        PhysicalMapSegment* segment = FindSegment(page);

        page = KernelToPhysPtr(page);
        uint64 index = ((uint64)page - segment->base) / 4096;

        for(uint64 i = 0; i < numPages; i++) {
            uint64 byte = (index + i) / 8;
            uint64 bit = (index + i) % 8;

            segment->map[byte] &= ~(1 << bit);
        }
    }

    void Init(KernelHeader* header)
    {
        g_PhysMap = header->physMap;
        g_PhysMapPages = (header->physMapSize + 4095) / 4096;
        g_PhysMapSegments = header->physMapSegments;
        g_HighMemBase = header->highMemoryBase;
        g_PML4 = header->pageBuffer;

        printf("Physical memory map at 0x%x\n", g_PhysMap);
        printf("Size needed for physical memory map: %i Bytes\n", g_PhysMapPages * 4096);

        uint64 availableMemory = 0;

        PhysicalMapSegment* physMap = g_PhysMap;
        for(uint64 s = 0; s < g_PhysMapSegments; s++) {
            availableMemory += physMap->numPages * 4096;
            physMap = (PhysicalMapSegment*)((char*)physMap + sizeof(PhysicalMapSegment) + (physMap->numPages + 7) / 8);
        }

        printf("Available memory: %i MB\n", availableMemory / 1024 / 1024);

        MarkUsed(header, (sizeof(KernelHeader) + 4095) / 4096);
        MarkUsed(header->kernelImage.buffer, (header->kernelImage.size + 4095) / 4096);
        MarkUsed(header->helloWorldImage.buffer, (header->helloWorldImage.size + 4095) / 4096);
        MarkUsed(header->fontImage.buffer, (header->fontImage.size + 4095) / 4096);
        MarkUsed(header->physMap, (header->physMapSize + 4095) / 4096);
        MarkUsed(header->stack, (header->stackSize + 4095) / 4096);
        MarkUsed(header->pageBuffer, (header->pageBufferSize + 4095) / 4096);

        printf("Physical memory map initialized\n");
    }

    void* AllocatePage()
    {
        return AllocatePages(1);
    }
    void FreePage(void* page)
    {
        FreePages(page, 1);
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
                        MarkUsed((void*)basePage, numPages);
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

}