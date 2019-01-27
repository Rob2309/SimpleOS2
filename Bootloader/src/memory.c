#include "memory.h"

#include "efiutil.h"

char* malloc(uint64 size)
{
    char* ret;
    g_EFISystemTable->BootServices->AllocatePool(EfiLoaderData, size, &ret);
    return ret;
}

void free(char* block)
{
    g_EFISystemTable->BootServices->FreePool(block);
}