#include "memory.h"

#include "efiutil.h"

char* malloc(uint64 size)
{
    size += sizeof(uint64);

    uint64 numPages = (size + 4095) / 4096 * 4096;
    uint64 addr = 1;

    EFI_STATUS err;
    err = g_EFISystemTable->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, numPages, &addr);
    if(err != EFI_SUCCESS)
        return 0;

    *(uint64*)addr = numPages;
    return addr + sizeof(uint64);
}

void free(char* block)
{
    block -= sizeof(uint64);
    uint64 numPages = *(uint64*)block;

    g_EFISystemTable->BootServices->FreePages(block, numPages);
}