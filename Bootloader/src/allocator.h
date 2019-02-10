#pragma once

#include "types.h"
#include "KernelHeader.h"

void* Allocate(uint64 size, MemoryType type);
void Free(void* block, uint64 size);