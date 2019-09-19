#pragma once

#include "types.h"

void* memcpy(void* dest, const void* src, uint64 num);
void* memmove(void* dest, const char* src, uint64 num);

char* strcpy(char* dest, const char* src);

char* strcat(char* dest, const char* src);

int64 strlen(const char* str);

int64 strcmp(const char* a, const char* b);