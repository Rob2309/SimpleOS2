#include "allocator.h"

#include "efiutil.h"

void* Allocate(uint64 size, EFI_MEMORY_TYPE type)
{
    void* res = (void*)1; // the pointer is not allowed to be 0 (no idea why)
    EFIUtil::SystemTable->BootServices->AllocatePages(AllocateAnyPages, type, (size + 4095) / 4096, (EFI_PHYSICAL_ADDRESS*)&res);
    return res;
}
void Free(void* block, uint64 size)
{
    EFIUtil::SystemTable->BootServices->FreePages((EFI_PHYSICAL_ADDRESS)block, (size + 4095) / 4096);
}

bool AllocateBelow(uint8** address, uint64 numPages, EFI_MEMORY_TYPE type) {
    if(EFIUtil::SystemTable->BootServices->AllocatePages(AllocateMaxAddress, type, numPages, (EFI_PHYSICAL_ADDRESS*)address) != EFI_SUCCESS)
        return false;
    return true;
}