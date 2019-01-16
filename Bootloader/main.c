#include <efi.h>
#include <efilib.h>

#include "util.h"
#include "file.h"

EFI_SYSTEM_TABLE* g_SystemTable;
EFI_HANDLE g_ImageHandle;

EFI_STATUS EFIAPI efi_main(EFI_HANDLE imgHandle, EFI_SYSTEM_TABLE* sysTable)
{
    g_SystemTable = sysTable;
    g_ImageHandle = imgHandle;

    EFI_STATUS err;

    err = sysTable->ConOut->ClearScreen(sysTable->ConOut);
    CHECK_ERROR(L"Failed to clear screen");

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