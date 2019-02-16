#include "efiutil.h"

#include "conio.h"
#include "allocator.h"

namespace EFIUtil {

    EFI_SYSTEM_TABLE* SystemTable;
    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics;

    void WaitForKey()
    {
        SystemTable->ConIn->Reset(SystemTable->ConIn, false);
        
        EFI_INPUT_KEY key;
        
        EFI_STATUS ret;

        while((ret = SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key)) == EFI_NOT_READY)
        ;
    }

    EfiMemoryMap GetMemoryMap()
    {
        UINTN memoryMapSize = 0;
        EFI_MEMORY_DESCRIPTOR* memMap;
        UINTN mapKey;
        UINTN descSize;
        UINT32 descVersion;
        EFI_STATUS err = EFIUtil::SystemTable->BootServices->GetMemoryMap(&memoryMapSize, memMap, &mapKey, &descSize, &descVersion);
        if(err != EFI_BUFFER_TOO_SMALL) {
            Console::Print(L"Failed to get memory map size\r\n");
            return { 0, 0, nullptr, 0, 0, 0 };
        }
        memMap = (EFI_MEMORY_DESCRIPTOR*)Allocate(memoryMapSize + 4096);
        memoryMapSize += 4096;
        err = EFIUtil::SystemTable->BootServices->GetMemoryMap(&memoryMapSize, memMap, &mapKey, &descSize, &descVersion);
        if(err != EFI_SUCCESS) {
            Console::Print(L"Failed to query memory map\r\n");
            return { 0, 0, nullptr, 0, 0, 0 };
        }

        return { memoryMapSize, memoryMapSize / descSize, memMap, descSize, descVersion, mapKey };
    }

}