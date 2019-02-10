#include "allocator.h"

#include "efiutil.h"

void* Allocate(uint64 size, MemoryType type)
{
    void* res = (void*)1;
    EFIUtil::SystemTable->BootServices->AllocatePages(AllocateAnyPages, (EFI_MEMORY_TYPE)type, (size + 4095) / 4096, (EFI_PHYSICAL_ADDRESS*)&res);
    return res;
}
void Free(void* block, uint64 size)
{
    EFIUtil::SystemTable->BootServices->FreePages((EFI_PHYSICAL_ADDRESS)block, (size + 4095) / 4096);
}