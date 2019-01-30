#pragma once

#include "types.h"

typedef void FILE;

FILE* fopen(char16* path);
void fclose(FILE* file);

int fread(uint8* buffer, uint64 bufferSize, FILE* file);

uint64 fgetsize(FILE* file);