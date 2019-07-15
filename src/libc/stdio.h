#pragma once

#include "types.h"

struct FILE;

extern FILE* stdout;
extern FILE* stderr;
extern FILE* stdin;

#define EOF -1

int remove(const char* path);

int fclose(FILE* file);
FILE* fopen(const char* path, const char* mode);
FILE* freopen(const char* path, const char* mode, FILE* oldFile);

int64 fread(void* buffer, int64 size, int64 count, FILE* file);
int64 fwrite(const void* buffer, int64 size, int64 count, FILE* file);

int64 fputs(const char* str, FILE* file);
int64 puts(const char* str);

void perror(const char* str);