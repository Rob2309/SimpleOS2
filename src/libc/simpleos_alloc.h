#pragma once

#include "simpleos_types.h"

int64 alloc_pages(void* addr, uint64 numPages);
int64 free_pages(void* addr, uint64 numPages);

void* malloc(int64 size);
void free(void* block);