#pragma once

#include "types.h"

void kmemset(void* dest, int value, uint64 size);
void kmemcpy(void* dest, const void* src, uint64 size);
void kmemmove(void* dest, const void* src, uint64 size);

void kmemcpyb(void* dest, const void* src, uint64 count);
void kmemcpyd(void* dest, const void* src, uint64 count);
void kmemcpyq(void* dest, const void* src, uint64 count);

/**
 * This function does the same as kmemcpy, except that it does not cause an exception when reading/writing to an invalid address.
 * Returns true if copying was successful, false if the buffer was invalid
 **/
bool kmemcpy_usersafe(void* dest, const void* src, uint64 count);
/**
 * This function does the same as kmemset, except that it does not cause an exception when writing to an invalid address.
 * Returns true if writing was successful, false if the buffer was invalid
 **/
bool kmemset_usersafe(void* dest, int value, uint64 size);

bool kpathcpy_usersafe(char* dest, const char* src);