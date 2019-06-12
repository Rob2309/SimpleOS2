#pragma once

#include "types.h"

void kmemset(void* dest, int value, uint64 size);
void kmemcpy(void* dest, const void* src, uint64 size);

void kmemcpyb(void* dest, const void* src, uint64 count);
void kmemcpyd(void* dest, const void* src, uint64 count);
void kmemcpyq(void* dest, const void* src, uint64 count);