#include <efi.h>

#include "file.h"
#include "console.h"
#include "conio.h"
#include "memory.h"
#include "ELF.h"

EFI_STATUS EFIAPI efi_main(EFI_HANDLE imgHandle, EFI_SYSTEM_TABLE* sysTable)
{
    EFI_STATUS err;

    EFIUtilInit(imgHandle, sysTable);
    ConsoleInit();

    printf("Loading ELF Image from disk...\n");

    FILE* elfFile = fopen(L"EFI\\BOOT\\KERNEL.ELF");
    if(elfFile == NULL) {
        printf("Failed to load KERNEL.ELF\n");
        WaitForKey();
        return 1;
    }

    uint64 elfFileSize = fgetsize(elfFile);
    char* elfFileData = malloc(elfFileSize);
    fread(elfFileData, elfFileSize, elfFile);
    fclose(elfFile);

    printf("Setting up ELF image...\n");

    uint64 elfProcessSize = GetELFSize(elfFileData);
    char* elfProcessBuffer = malloc(elfProcessSize);

    Elf64Addr entryPoint;
    bool success = PrepareELF(elfFileData, elfProcessBuffer, elfProcessBuffer, &entryPoint);
    if(success == false) {
        printf("Failed to prepare Kernel process\n");
        WaitForKey();
        return 1;
    }

    typedef int (*MAINFUNC)(int argc, char** argv);
    MAINFUNC kernelMain = (MAINFUNC)entryPoint;

    printf("Running Kernel process...\n");
    char* args = &printf;
    int ret = kernelMain(1, &args);
    printf("Process returned %i\n", ret);

    printf("Press any key to continue...\n");
    WaitForKey();

    ConsoleCleanup();
    return EFI_SUCCESS;
}