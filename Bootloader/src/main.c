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
    printf("Elf file size: %i\n", elfFileSize);
    char* elfFileData = malloc(elfFileSize);
    fread(elfFileData, elfFileSize, elfFile);
    fclose(elfFile);

    uint64 check = 0;
    for(uint64 i = 0; i < elfFileSize; i++)
        check += elfFileData[i];

    printf("Checksum: %x\n", check);

    printf("Setting up ELF image...\n");

    uint64 elfProcessSize = GetELFSize(elfFileData);
    printf("Elf Process size: %x\n", elfProcessSize);
    char* elfProcessBuffer = malloc(elfProcessSize);

    Elf64Addr entryPoint;
    bool success = PrepareELF(elfFileData, elfProcessBuffer, elfProcessBuffer, &entryPoint);
    if(success == false) {
        printf("Failed to prepare Kernel process\n");
        WaitForKey();
        return 1;
    }

    free(elfFileData);

    UINTN memMapSize = 4096;
    EFI_MEMORY_DESCRIPTOR* memMap = malloc(memMapSize);
    UINTN memMapKey;
    UINTN descSize;
    UINT32 descVersion;
    g_EFISystemTable->BootServices->GetMemoryMap(&memMapSize, memMap, &memMapKey, &descSize, &descVersion);
    g_EFISystemTable->BootServices->ExitBootServices(g_EFILoadedImage, memMapKey);


    KernelHeader kernelHeader;
    kernelHeader.printf = &printf;
    kernelHeader.memMapLength = memMapSize / descSize;
    kernelHeader.memMapDescriptorSize = descSize;
    kernelHeader.memMap = memMap;

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