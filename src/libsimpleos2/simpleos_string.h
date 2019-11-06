#pragma once

#include "simpleos_types.h"

/**
 * Copies the buffer given by src into dest.
 * @note the buffers may only overlap if dest <= src
 **/
void* memcpy(void* dest, const void* src, int64 num);
/**
 * Copies the buffer given by src into dest.
 * @note the buffers may overlap in any way
 **/
void* memmove(void* dest, const void* src, int64 num);

/**
 * Copies a string given by src into dest.
 **/
char* strcpy(char* dest, const char* src);

/**
 * Appends src to dest.
 **/
char* strcat(char* dest, const char* src);

/**
 * Returns the length of the given string.
 * @note The 0 terminator is not counted.
 **/
int64 strlen(const char* str);

/**
 * Compares two string.
 * @returns     - 0 if the strings are equal
 *              - a negative value if a is alphabetically smaller than b
 *              - a positive value if a is alphabetically larger than b
 **/
int64 strcmp(const char* a, const char* b);