#pragma once

#include <efi.h>
#include "types.h"

// Convenience structure for holding and accessing a memory map
struct EfiMemoryMap
{
    UINTN size;                     // size of the memory map buffer
    UINTN length;                   // number of descriptors in the memory map
    EFI_MEMORY_DESCRIPTOR* entries; // pointer to the array of entries
    UINTN entrySize;                // size of each entry
    UINT32 version;                 // version of the entries
    UINTN key;                      // key of the memory map

    inline EFI_MEMORY_DESCRIPTOR& operator[] (uint64 index)
    {
        return *(EFI_MEMORY_DESCRIPTOR*)((char*)entries + index * entrySize);
    }
};

namespace EFIUtil {

    extern EFI_SYSTEM_TABLE* SystemTable;
    extern EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
    extern EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;
    extern EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics;

    void WaitForKey();

    EfiMemoryMap GetMemoryMap();

}