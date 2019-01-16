#include <efi.h>

#include "util.h"
#include "file.h"
#include "efiutil.h"

EFI_STATUS EFIAPI efi_main(EFI_HANDLE imgHandle, EFI_SYSTEM_TABLE* sysTable)
{
    EFI_STATUS err;

    EFIUtilInit(imgHandle, sysTable);

    err = sysTable->ConOut->ClearScreen(sysTable->ConOut);
    CHECK_ERROR(L"Failed to clear screen");

    UINTN memMapSize = 0;
    EFI_MEMORY_DESCRIPTOR* memMap;
    UINTN memMapKey;
    UINTN memMapDescSize;
    UINT32 memMapDescVersion;
    err = g_EFISystemTable->BootServices->GetMemoryMap(&memMapSize, memMap, &memMapKey, &memMapDescSize, &memMapDescVersion);
    if(err == EFI_BUFFER_TOO_SMALL)
    {
        int numPages = (memMapSize + 4095) / 4096 * 4096;
        memMapSize = numPages * 4096;
        UINT64 addr = 1;
        err = g_EFISystemTable->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, numPages, &addr);
        CHECK_ERROR_HALT(L"Failed to allocate buffer for memory map");

        memMap = addr;
    }
    err = g_EFISystemTable->BootServices->GetMemoryMap(&memMapSize, memMap, &memMapKey, &memMapDescSize, &memMapDescVersion);
    CHECK_ERROR_HALT(L"Failed to get memory map");

    int numEntries = memMapSize / memMapDescSize;
    Print(L"MemMapSize: "); Print(ToString(memMapSize));
    Print(L"\r\nDescriptorSize: "); Print(ToString(memMapDescSize));
    Print(L"\r\nNumEntries: "); Print(ToString(numEntries));
    Print(L"\r\n");

    FILE* txtFile = fopen(L"EFI\\BOOT\\msg.txt");
    if(txtFile == 0)
    {
        Println(L"Failed to open msg file");
        return EFI_ABORTED;
    }

    UINTN bufSize = 256;
    char buffer[bufSize + 1];
    fread(buffer, bufSize, txtFile);
    CHECK_ERROR(L"Failed to read file");

    fclose(txtFile);

    CHAR16 convBuffer[bufSize + 1];
    WidenString(convBuffer, buffer);

    Println(convBuffer);

    WaitForKey();

    return EFI_SUCCESS;
}