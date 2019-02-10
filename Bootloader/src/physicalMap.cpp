#include "physicalMap.h"

#include "efiutil.h"
#include "allocator.h"

namespace PhysicalMap {
    
    static void CleanupMemMap(EFI_MEMORY_DESCRIPTOR* map, uint64& count, uint64 entrySize)
    {
        // Make every usable memory segment have type ConventionalMemory
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

    PhysMapInfo Build()
    {
        UINTN memoryMapSize = 0;
        EFI_MEMORY_DESCRIPTOR* memMap;
        UINTN mapKey;
        UINTN descSize;
        UINT32 descVersion;
        
        EFI_STATUS err = EFIUtil::SystemTable->BootServices->GetMemoryMap(&memoryMapSize, memMap, &mapKey, &descSize, &descVersion);
        memMap = (EFI_MEMORY_DESCRIPTOR*)Allocate(memoryMapSize + 4096);
        memoryMapSize += 4096;
        err = EFIUtil::SystemTable->BootServices->GetMemoryMap(&memoryMapSize, memMap, &mapKey, &descSize, &descVersion);

        uint64 length = memoryMapSize / descSize;
        CleanupMemMap(memMap, length, descSize);

        uint64 sizeNeeded = 0;
        uint64 numSegments = 0;

        for(uint64 i = 0; i < length; i++) {
            EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((char*)memMap + i * descSize);
            
            if(desc->Type == EfiConventionalMemory)
            {
                numSegments++;

                sizeNeeded += sizeof(PhysicalMapSegment);
                sizeNeeded += (desc->NumberOfPages + 7) / 8;
            }
        }

        PhysicalMapSegment* physMapBase = (PhysicalMapSegment*)Allocate(sizeNeeded);
        PhysicalMapSegment* physMap = physMapBase;
         for(uint64 i = 0; i < length; i++) {
            EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((char*)memMap + i * descSize);
            if(desc->Type == EfiConventionalMemory)
            {
                physMap->base = desc->PhysicalStart;
                physMap->numPages = desc->NumberOfPages;

                uint64 numBytes = (desc->NumberOfPages + 7) / 8;
                for(int b = 0; b < numBytes; b++)
                    physMap->map[b] = 0;

                physMap = (PhysicalMapSegment*)((char*)physMap + sizeof(PhysicalMapSegment) + (physMap->numPages + 7) / 8);
            }
        }

        return { physMapBase, sizeNeeded, numSegments };
    }

}