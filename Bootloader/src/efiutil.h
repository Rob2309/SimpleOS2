#pragma once

#include <efi.h>
#include "types.h"

struct EfiMemoryMap
{
    UINTN size;
    UINTN length;
    EFI_MEMORY_DESCRIPTOR* entries;
    UINTN entrySize;
    UINT32 version;
    UINTN key;

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