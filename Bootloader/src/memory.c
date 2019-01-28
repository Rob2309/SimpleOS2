#include "memory.h"

#include "efiutil.h"

char* malloc(uint64 size)
{
    char* ret;
    EFI_STATUS err = g_EFISystemTable->BootServices->AllocatePool(EfiLoaderData, size, &ret);
    if(err != EFI_SUCCESS) {
        printf("malloc failed\n");
    }
    return ret;
}

void free(char* block)
{
    g_EFISystemTable->BootServices->FreePool(block);
}