#pragma once

#include "types.h"

int64 alloc_pages(void* addr, uint64 numPages);
int64 free_pages(void* addr, uint64 numPages);