#pragma once

#include "simpleos_types.h"

/**
 * Maps the number of contiguous pages given by numPages into the memory space, starting at addr
 **/
int64 alloc_pages(void* addr, uint64 numPages);
/**
 * Unmaps the given pages and returns them to the operating system.
 * @note addr and numPages do not have to match a previous call to alloc_pages
 **/
int64 free_pages(void* addr, uint64 numPages);

/**
 * Works just like malloc in the standard C Library
 **/
void* malloc(int64 size);
/**
 * Works just like free in the standard C Library
 **/
void free(void* block);