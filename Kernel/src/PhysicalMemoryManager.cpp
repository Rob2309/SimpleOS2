#include "PhysicalMemoryManager.h"

#include "conio.h"

namespace PhysicalMemoryManager {

    struct MemSegment {
        uint64 base;
        uint64 numPages;
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

    static void MarkUnavailable(uint64 addr, uint64 numPages)
    {
        MemSegment* seg = FindSegment(addr);
        
        uint64 basePage = (addr - seg->base) / 4096;
        uint64 pagesInSegment = seg->numPages - basePage;
        for(int i = 0; i < pagesInSegment; i++)
        {
            int byte = (i + basePage) / 8;
            int bit = (i + basePage) % 8;

            seg->map[byte] |= (1 << bit);
        }

        if(pagesInSegment < numPages) {
            MarkUnavailable(addr + pagesInSegment * 4096, numPages - pagesInSegment);
        }
    }

    void Init(KernelHeader* header)
    {
        uint64 sizeNeeded = 0;
        uint64 availableMemory = 0;

        for(uint64 i = 0; i < header->memMapLength; i++) {
            MemoryDescriptor* desc = (MemoryDescriptor*)((char*)header->memMap + i * header->memMapDescriptorSize);
            
            if(desc->type == MemoryType::LoaderCode ||
                desc->type == MemoryType::LoaderData ||
                desc->type == MemoryType::BootServicesCode ||
                desc->type == MemoryType::BootServicesData ||
                desc->type == MemoryType::ConventionalMemory)
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

        // initialize memMap
        MemSegment* physMap = g_PhysMap;
        for(uint64 i = 0; i < header->memMapLength; i++) {
            MemoryDescriptor* desc = (MemoryDescriptor*)((char*)header->memMap + i * header->memMapDescriptorSize);
            if(desc->type == MemoryType::LoaderCode ||
                desc->type == MemoryType::LoaderData ||
                desc->type == MemoryType::BootServicesCode ||
                desc->type == MemoryType::BootServicesData ||
                desc->type == MemoryType::ConventionalMemory)
            {
                physMap->base = desc->physicalStart;
                physMap->numPages = desc->numPages;

                uint64 numBytes = (desc->numPages + 7) / 8;
                for(int b = 0; b < numBytes; b++)
                    physMap->map[b] = 0;

                physMap = (MemSegment*)((char*)physMap + sizeof(MemSegment) + (physMap->numPages + 7) / 8);
            }
        }
        
        // mark memory map as unavailable
        MarkUnavailable((uint64)g_PhysMap, pagesNeeded);
        printf("Marked physical memory map as unavailable memory\n");
        // mark header as unavailable memory
        MarkUnavailable((uint64)header, (sizeof(KernelHeader) + 4095) / 4096);
        printf("Marked header as unavailable memory\n");
        // mark module descriptors as unavailable
        MarkUnavailable((uint64)header->modules, (header->numModules * sizeof(ModuleDescriptor) + 4095) / 4096);
        printf("Marked module map as unavailable memory\n");
        // mark module buffers as unavailable
        for(int i = 0; i < header->numModules; i++)
            MarkUnavailable((uint64)(header->modules[i].buffer), (header->modules[i].size + 4095) / 4096);
        printf("Marked modules as unavailable memory\n");

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
        
    }
    void FreePages(void* pages, uint64 numPages)
    {
        
    }

}