#include "PhysicalMemoryManager.h"

#include "conio.h"

namespace PhysicalMemoryManager {

    static PhysicalMapSegment* g_PhysMap = nullptr;
    static uint64 g_PhysMapPages = 0;
    static uint64 g_PhysMapSegments;

    void* GetPhysicalMapPointer()
    {
        return g_PhysMap;
    }
    uint64 GetPhysicalMapPageCount()
    {
        return g_PhysMapPages;
    }

    static PhysicalMapSegment* FindSegment(uint64 pageAddress) {
        PhysicalMapSegment* physMap = g_PhysMap;

        for(uint64 s = 0; s < g_PhysMapSegments; s++) {
            if(physMap->base <= pageAddress && (physMap->base + physMap->numPages * 4096 - 4096) >= pageAddress) {
                return physMap;
            } else {
                physMap = (PhysicalMapSegment*)((char*)physMap + sizeof(PhysicalMapSegment) + (physMap->numPages + 7) / 8);
            }
        }

        return nullptr;
    }

    static void MarkUsed(PhysicalMapSegment* segment, uint64 page, uint64 numPages) {
        for(uint64 i = 0; i < numPages; i++) {
            uint64 byte = (page + i) / 8;
            uint64 bit = (page + i) % 8;

            segment->map[byte] |= (1 << bit);
        }
    }

    static void MarkFree(PhysicalMapSegment* segment, uint64 page, uint64 numPages) {
        for(uint64 i = 0; i < numPages; i++) {
            uint64 byte = (page + i) / 8;
            uint64 bit = (page + i) % 8;

            segment->map[byte] &= ~(1 << bit);
        }
    }

    void Init(KernelHeader* header)
    {
        g_PhysMap = header->physMap;
        g_PhysMapPages = (header->physMapSize + 4095) / 4096;
        g_PhysMapSegments = header->physMapSegments;

        printf("Physical memory map at 0x%x\n", g_PhysMap);
        printf("Size needed for physical memory map: %i Bytes\n", g_PhysMapPages * 4096);

        uint64 availableMemory = 0;

        PhysicalMapSegment* physMap = g_PhysMap;
        for(uint64 s = 0; s < g_PhysMapSegments; s++) {
            availableMemory += physMap->numPages * 4096;
            physMap = (PhysicalMapSegment*)((char*)physMap + sizeof(PhysicalMapSegment) + (physMap->numPages + 7) / 8);
        }

        printf("Available memory: %i MB\n", availableMemory / 1024 / 1024);

        PhysicalMapSegment* seg = FindSegment((uint64)header);
        MarkUsed(seg, ((uint64)header - seg->base) / 4096, (sizeof(KernelHeader) + 4095) / 4096);
        seg = FindSegment((uint64)(header->kernelImage.buffer));
        MarkUsed(seg, ((uint64)(header->kernelImage.buffer) - seg->base) / 4096, (header->kernelImage.size + 4095) / 4096);
        seg = FindSegment((uint64)(header->helloWorldImage.buffer));
        MarkUsed(seg, ((uint64)(header->helloWorldImage.buffer) - seg->base) / 4096, (header->helloWorldImage.size + 4095) / 4096);
        seg = FindSegment((uint64)(header->fontImage.buffer));
        MarkUsed(seg, ((uint64)(header->fontImage.buffer) - seg->base) / 4096, (header->fontImage.size + 4095) / 4096);
        seg = FindSegment((uint64)(header->physMap));
        MarkUsed(seg, ((uint64)(header->physMap) - seg->base) / 4096, (header->physMapSize + 4095) / 4096);
        seg = FindSegment((uint64)(header->stack));
        MarkUsed(seg, ((uint64)(header->stack) - seg->base) / 4096, (header->stackSize + 4095) / 4096);

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
        PhysicalMapSegment* seg = FindSegment((uint64)pages);
        MarkFree(seg, ((uint64)pages - seg->base) / 4096, numPages);
    }

}