#include <efi.h>

#include "file.h"
#include "console.h"
#include "conio.h"

EFI_STATUS EFIAPI efi_main(EFI_HANDLE imgHandle, EFI_SYSTEM_TABLE* sysTable)
{
    EFI_STATUS err;

    EFIUtilInit(imgHandle, sysTable);
    ConsoleInit();

    printf("Hello World\nThis is a test\n");

    int l = 0;
    while(1) {
        WaitForKey();
        printf("Line %i\n", l);
        l++;
    }

    ConsoleCleanup();
    return EFI_SUCCESS;
}