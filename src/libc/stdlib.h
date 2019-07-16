#pragma once

#include "types.h"

void* malloc(int64 size);
void free(void* block);
void* calloc(uint64 num, uint64 size);
void* realloc(void* ptr, uint64 size);

void abort();

int atexit(void (*func)());
int at_quick_exit(void (*func)());

void exit(int status);
void quick_exit(int status);
void _Exit(int status);

char* getenv(const char* name);

int system(const char* command);

int abs(int n);