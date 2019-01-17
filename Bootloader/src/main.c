#include <efi.h>

#include "util.h"
#include "file.h"
#include "efiutil.h"
#include "memory.h"
#include "conio.h"

EFI_STATUS EFIAPI efi_main(EFI_HANDLE imgHandle, EFI_SYSTEM_TABLE* sysTable)
{
    EFI_STATUS err;

    EFIUtilInit(imgHandle, sysTable);

    err = sysTable->ConOut->ClearScreen(sysTable->ConOut);
    CHECK_ERROR("Failed to clear screen\n");

    UINTN memMapSize = 0;
    EFI_MEMORY_DESCRIPTOR* memMap;
    UINTN memMapKey;
    UINTN memMapDescSize;
    UINT32 memMapDescVersion;
    err = g_EFISystemTable->BootServices->GetMemoryMap(&memMapSize, memMap, &memMapKey, &memMapDescSize, &memMapDescVersion);
    if(err == EFI_BUFFER_TOO_SMALL)
    {
        memMapSize += memMapDescSize;
        memMap = malloc(memMapSize);

        if(memMap == NULL) {
            printf("Failed to allocate buffer for memory map\n");
            while(1);
        }

        err = g_EFISystemTable->BootServices->GetMemoryMap(&memMapSize, memMap, &memMapKey, &memMapDescSize, &memMapDescVersion);
        if(err == EFI_INVALID_PARAMETER) {
            printf("Invalid arg\n");
        }
        if(err == EFI_BUFFER_TOO_SMALL) {
            printf("Small buffer\n");
        }
        CHECK_ERROR_HALT("Failed to get memory map\n");
    }

    uint64 memSize = 0;

    int numEntries = memMapSize / memMapDescSize;
    for(int i = 0; i < numEntries; i++) {
        switch(memMap->Type)
        {
        case EfiLoaderCode:
        case EfiLoaderData:
        case EfiBootServicesCode:
        case EfiBootServicesData:
        case EfiConventionalMemory:
            memSize += memMap->NumberOfPages * 4096;
            break;
        }

        memMap = (char*)memMap + memMapDescSize;
    }

    printf("Available memory: %i MB\n", memSize / 1024 / 1024);

    WaitForKey();

    return EFI_SUCCESS;
}