#include <efi.h>

#include "file.h"
#include "console.h"
#include "conio.h"
#include "memory.h"
#include "ELF.h"
#include "efiutil.h"

#include <KernelHeader.h>

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


    UINTN memMapSize = 4096;
    EFI_MEMORY_DESCRIPTOR* memMap = malloc(memMapSize);
    UINTN memMapKey;
    UINTN descSize;
    UINT32 descVersion;
    g_EFISystemTable->BootServices->GetMemoryMap(&memMapSize, memMap, &memMapKey, &descSize, &descVersion);
    g_EFISystemTable->BootServices->ExitBootServices(g_EFILoadedImage, memMapKey);

    KernelHeader kernelHeader;
    kernelHeader.printf = &printf;
    kernelHeader.memMapLength = 0;
    kernelHeader.memMap = NULL;

    typedef int (*MAINFUNC)(KernelHeader* header);
    MAINFUNC kernelMain = (MAINFUNC)entryPoint;

    printf("Running Kernel process...\n");
    char* args = &printf;
    int ret = kernelMain(&kernelHeader);
    printf("Process returned %i\n", ret);

    while(true) ;

    //ConsoleCleanup();
    return EFI_SUCCESS;
}