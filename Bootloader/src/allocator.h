#pragma once

#include "types.h"
#include "KernelHeader.h"

// Allocates size bytes, rounded up to the next page
void* Allocate(uint64 size);

void Free(void* block, uint64 size);