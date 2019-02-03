#pragma once

#include "types.h"

void* Allocate(uint64 size);
void Free(void* block, uint64 size);