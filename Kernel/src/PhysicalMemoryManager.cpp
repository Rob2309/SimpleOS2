#include "PhysicalMemoryManager.h"

#include "conio.h"

namespace PhysicalMemoryManager {

    static void MarkUnavailable(uint64 addr, uint64 numPages)
    {

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
                sizeNeeded += sizeof(uint64) * 2;
                sizeNeeded += (desc->numPages + 7) / 8;

                availableMemory += desc->numPages * 4096;
            }
        }

        printf("Available memory: %i MB\n", availableMemory / 1024 / 1024);
        printf("Size needed for physical memory map: %i Bytes\n", sizeNeeded);

        uint64 pagesNeeded = (sizeNeeded + 4095) / 4096;
        printf("%i pages required for physical memory map\n", pagesNeeded);

        // mark header as unavailable memory
        MarkUnavailable((uint64)header, (sizeof(KernelHeader) + 4095) / 4096);
    }

}