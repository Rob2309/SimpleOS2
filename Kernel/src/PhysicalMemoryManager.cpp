#include "PhysicalMemoryManager.h"

#include "conio.h"

namespace PhysicalMemoryManager {

    struct MemSegment {
        uint64 base;
        uint64 numPages;
        uint64 numFreePages;
        uint8 map[];
    };

    static MemSegment* g_PhysMap = nullptr;
    static uint64 g_PhysMapSegments;

    static MemSegment* FindSegment(uint64 pageAddress) {
        MemSegment* physMap = g_PhysMap;

        for(uint64 s = 0; s < g_PhysMapSegments; s++) {
            if(physMap->base <= pageAddress && (physMap->base + physMap->numPages * 4096 - 4096) >= pageAddress) {
                return physMap;
            } else {
                physMap = (MemSegment*)((char*)physMap + sizeof(MemSegment) + (physMap->numPages + 7) / 8);
            }
        }

        return nullptr;
    }

    static void MarkUsed(MemSegment* segment, uint64 page, uint64 numPages) {
        for(uint64 i = 0; i < numPages; i++) {
            uint64 byte = (page + i) / 8;
            uint64 bit = (page + i) % 8;

            segment->map[byte] |= (1 << bit);
            segment->numFreePages--;
        }
    }

    static void MarkFree(MemSegment* segment, uint64 page, uint64 numPages) {
        for(uint64 i = 0; i < numPages; i++) {
            uint64 byte = (page + i) / 8;
            uint64 bit = (page + i) % 8;

            segment->map[byte] &= ~(1 << bit);
            segment->numFreePages++;
        }
    }

    static void CleanupMemMap(KernelHeader* header)
    {
        // Make every usable memory segment have type ConventionalMemory
        for(uint64 s = 0; s < header->memMapLength; s++) {
            MemoryDescriptor* seg = (MemoryDescriptor*)((char*)(header->memMap) + s * header->memMapDescriptorSize);
            if(seg->type == MemoryType::LoaderCode ||
                seg->type == MemoryType::LoaderData ||
                seg->type == MemoryType::BootServicesCode ||
                seg->type == MemoryType::BootServicesData)
            {
                seg->type = MemoryType::ConventionalMemory;
            }
        }

        // join adjacent usable segments
        for(uint64 s = 0; s < header->memMapLength; s++) {
            MemoryDescriptor* seg = (MemoryDescriptor*)((char*)(header->memMap) + s * header->memMapDescriptorSize);
            if(seg->type != MemoryType::ConventionalMemory)
                continue;
            if(seg->physicalStart == (uint64)-1)  // segment was joined with another segment
                continue;

            uint64 start = seg->physicalStart;
            uint64 end = seg->physicalStart + seg->numPages * 4096;

            // search for adjacent segments
            for(uint64 c = s + 1; c < header->memMapLength; c++) {
                MemoryDescriptor* cmp = (MemoryDescriptor*)((char*)(header->memMap) + c * header->memMapDescriptorSize);
                if(cmp->type != MemoryType::ConventionalMemory)
                    continue;
                if(cmp->physicalStart == (uint64)-1)  // segment was joined with another segment
                    continue;

                uint64 cmpStart = cmp->physicalStart;
                uint64 cmpEnd = cmp->physicalStart + cmp->numPages * 4096;

                // cmp after seg
                if(cmpStart == end) {
                    seg->numPages += cmp->numPages;
                    cmp->physicalStart = (uint64)-1;
                    s--; // search for further adjacent entries
                    break;
                }  // cmp before seg 
                else if(cmpEnd == start) {
                    seg->physicalStart = cmp->physicalStart;
                    seg->numPages += cmp->numPages;
                    cmp->physicalStart = (uint64)-1;
                    s--;
                    break;
                }
            }
        }

        // remove unused memorymap entries
        for(uint64 s = 0; s < header->memMapLength; s++) {
            MemoryDescriptor* seg = (MemoryDescriptor*)((char*)(header->memMap) + s * header->memMapDescriptorSize);
            if(seg->physicalStart != (uint64)-1)
                continue;

            for(uint64 m = s + 1; m < header->memMapLength; m++) {
                MemoryDescriptor* dest = (MemoryDescriptor*)((char*)(header->memMap) + (m - 1) * header->memMapDescriptorSize);
                MemoryDescriptor* src = (MemoryDescriptor*)((char*)(header->memMap) + m * header->memMapDescriptorSize);

                *dest = *src;
            }

            header->memMapLength--;
            s--;
        }
    }

    void Init(KernelHeader* header)
    {
        uint64 sizeNeeded = 0;
        uint64 availableMemory = 0;

        for(uint64 i = 0; i < header->memMapLength; i++) {
            MemoryDescriptor* desc = (MemoryDescriptor*)((char*)header->memMap + i * header->memMapDescriptorSize);
            
            if(desc->type == MemoryType::ConventionalMemory ||
                desc->type == MemoryType::BootServicesCode ||
                desc->type == MemoryType::BootServicesData ||
                desc->type == MemoryType::LoaderCode ||
                desc->type == MemoryType::LoaderData)
            {
                g_PhysMapSegments++;

                sizeNeeded += sizeof(MemSegment);
                sizeNeeded += (desc->numPages + 7) / 8;

                availableMemory += desc->numPages * 4096;
            }
        }

        printf("Available memory: %i MB\n", availableMemory / 1024 / 1024);
        printf("Size needed for physical memory map: %i Bytes\n", sizeNeeded);

        uint64 pagesNeeded = (sizeNeeded + 4095) / 4096;
        printf("%i pages required for physical memory map\n", pagesNeeded);

        // find spot for memory map
        for(int i = 0; i < header->memMapLength; i++) {
            MemoryDescriptor* desc = (MemoryDescriptor*)((char*)header->memMap + i * header->memMapDescriptorSize);
            if(desc->type != MemoryType::ConventionalMemory)
                continue;

            if(desc->physicalStart != 0 && desc->numPages >= pagesNeeded) {
                g_PhysMap = (MemSegment*)desc->physicalStart;
                break;
            }
        }

        if(g_PhysMap == nullptr) {
            printf("Failed to find location for memory map\n");
            while(true);
        }

        printf("Physical memory map at 0x%x\n", g_PhysMap);

        CleanupMemMap(header);

        // initialize memMap
        MemSegment* physMap = g_PhysMap;
        for(uint64 i = 0; i < header->memMapLength; i++) {
            MemoryDescriptor* desc = (MemoryDescriptor*)((char*)header->memMap + i * header->memMapDescriptorSize);
            if(desc->type == MemoryType::ConventionalMemory)
            {
                physMap->base = desc->physicalStart;
                physMap->numPages = desc->numPages;
                physMap->numFreePages = desc->numPages;

                uint64 numBytes = (desc->numPages + 7) / 8;
                for(int b = 0; b < numBytes; b++)
                    physMap->map[b] = 0;

                physMap = (MemSegment*)((char*)physMap + sizeof(MemSegment) + (physMap->numPages + 7) / 8);
            }
        }
        
        // mark physical memory map as unavailable
        MemSegment* seg = FindSegment((uint64)g_PhysMap);
        MarkUsed(seg, ((uint64)g_PhysMap - seg->base) / 4096, pagesNeeded);
        // mark header as unavailable memory
        seg = FindSegment((uint64)header);
        MarkUsed(seg, ((uint64)header - seg->base) / 4096, (sizeof(KernelHeader) + 4095) / 4096);
        // mark module descriptors as unavailable
        seg = FindSegment((uint64)header->modules);
        MarkUsed(seg, ((uint64)(header->modules) - seg->base) / 4096, (header->numModules * sizeof(ModuleDescriptor) + 4095) / 4096);
        // mark module buffers as unavailable
        for(int i = 0; i < header->numModules; i++) {
            seg = FindSegment((uint64)(header->modules[i].buffer));
            MarkUsed(seg, ((uint64)(header->modules[i].buffer) - seg->base) / 4096, (header->modules[i].size + 4095) / 4096);
        }
        // mark header memory map as unavailable
        seg = FindSegment((uint64)(header->memMap));
        MarkUsed(seg, ((uint64)(header->memMap) - seg->base) / 4096, (header->memMapLength * header->memMapDescriptorSize + 4095) / 4096);

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
        MemSegment* seg = g_PhysMap;
        for(int s = 0; s < g_PhysMapSegments; s++) {
            if(seg->numFreePages >= numPages) 
            {
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
            }

            seg = (MemSegment*)((char*)seg + sizeof(MemSegment) + (seg->numPages + 7) / 8);
        }

        return nullptr;
    }
    void FreePages(void* pages, uint64 numPages)
    {
        MemSegment* seg = FindSegment((uint64)pages);
        MarkFree(seg, ((uint64)pages - seg->base) / 4096, numPages);
    }

}