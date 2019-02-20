#pragma once

#include "types.h"
#include "KernelHeader.h"
#include "efi.h"

void* Allocate(uint64 size, EFI_MEMORY_TYPE type = EfiLoaderData);
void Free(void* block, uint64 size);