#include "physicalMap.h"

#include "efiutil.h"
#include "allocator.h"
#include "paging.h"

namespace PhysicalMap {
    
    static void CleanupMemMap(EFI_MEMORY_DESCRIPTOR* map, uint64& count, uint64 entrySize)
    {
        // Make every usable memory segment have type ConventionalMemory
        // Make every other segment have Type UnusableMemory
        for(uint64 s = 0; s < count; s++) {
            EFI_MEMORY_DESCRIPTOR* seg = (EFI_MEMORY_DESCRIPTOR*)((char*)(map) + s * entrySize);
            if(seg->Type == EfiLoaderCode ||
                seg->Type == EfiLoaderData ||
                seg->Type == EfiBootServicesCode ||
                seg->Type == EfiBootServicesData ||
                seg->Type == EfiConventionalMemory)
            {
                seg->Type = EfiConventionalMemory;
            }
            else {
                seg->Type = EfiUnusableMemory;
                seg->PhysicalStart = (uint64)-1;
            }
        }

        // join adjacent usable segments
        for(uint64 s = 0; s < count; s++) {
            EFI_MEMORY_DESCRIPTOR* seg = (EFI_MEMORY_DESCRIPTOR*)((char*)(map) + s * entrySize);
            if(seg->Type != EfiConventionalMemory)
                continue;
            if(seg->PhysicalStart == (uint64)-1)  // segment was joined with another segment
                continue;

            uint64 start = seg->PhysicalStart;
            uint64 end = seg->PhysicalStart + seg->NumberOfPages * 4096;

            // search for adjacent segments
            for(uint64 c = s + 1; c < count; c++) {
                EFI_MEMORY_DESCRIPTOR* cmp = (EFI_MEMORY_DESCRIPTOR*)((char*)(map) + c * entrySize);
                if(cmp->Type != EfiConventionalMemory)
                    continue;
                if(cmp->PhysicalStart == (uint64)-1)  // segment was joined with another segment
                    continue;

                uint64 cmpStart = cmp->PhysicalStart;
                uint64 cmpEnd = cmp->PhysicalStart + cmp->NumberOfPages * 4096;

                // cmp after seg
                if(cmpStart == end) {
                    seg->NumberOfPages += cmp->NumberOfPages;
                    cmp->PhysicalStart = (uint64)-1;
                    s--; // search for further adjacent entries
                    break;
                }  // cmp before seg 
                else if(cmpEnd == start) {
                    seg->PhysicalStart = cmp->PhysicalStart;
                    seg->NumberOfPages += cmp->NumberOfPages;
                    cmp->PhysicalStart = (uint64)-1;
                    s--;
                    break;
                }
            }
        }

        // remove unused memorymap entries
        for(uint64 s = 0; s < count; s++) {
            EFI_MEMORY_DESCRIPTOR* seg = (EFI_MEMORY_DESCRIPTOR*)((char*)(map) + s * entrySize);
            if(seg->PhysicalStart != (uint64)-1)
                continue;

            for(uint64 m = s + 1; m < count; m++) {
                EFI_MEMORY_DESCRIPTOR* dest = (EFI_MEMORY_DESCRIPTOR*)((char*)(map) + (m - 1) * entrySize);
                EFI_MEMORY_DESCRIPTOR* src = (EFI_MEMORY_DESCRIPTOR*)((char*)(map) + m * entrySize);

                *dest = *src;
            }

            count--;
            s--;
        }
    }

    PhysicalMapSegment* Build(EfiMemoryMap memMap)
    {
        CleanupMemMap(memMap.entries, memMap.length, memMap.entrySize);

        uint64 sizeNeeded = 0;
        uint64 numSegments = 0;

        PhysicalMapSegment* firstSegment = nullptr;
        PhysicalMapSegment* lastSegment = nullptr;

        for(uint64 i = 0; i < memMap.length; i++) {
            EFI_MEMORY_DESCRIPTOR& desc = memMap[i];
            
            if(desc.Type == EfiConventionalMemory)
            {
                // Just store this structure in the physical page it represents
                PhysicalMapSegment* seg = (PhysicalMapSegment*)Paging::ConvertPtr((void*)desc.PhysicalStart);

                // Insert it into the list
                if(firstSegment == nullptr) {
                    firstSegment = seg;
                    lastSegment = firstSegment;

                    firstSegment->base = desc.PhysicalStart;
                    firstSegment->numPages = desc.NumberOfPages;
                    firstSegment->prev = nullptr;
                    firstSegment->next = nullptr;
                } else {
                    lastSegment->next = seg;
                    seg->prev = lastSegment;
                    lastSegment = seg;
                    
                    seg->base = desc.PhysicalStart;
                    seg->numPages = desc.NumberOfPages;
                    seg->next = nullptr;
                }
            }
        }

        return firstSegment;
    }

}