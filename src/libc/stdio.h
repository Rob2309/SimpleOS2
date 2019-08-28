#pragma once

#include "types.h"

#include "simpleos_vfs.h"

struct FILE;

#define stdinfd 0
#define stdoutfd 1
#define stderrfd 2

extern FILE* stdout;
extern FILE* stderr;
extern FILE* stdin;

#define EOF -1
#define BUFSIZ 4096

int remove(const char* path);

int fclose(FILE* file);
FILE* fopen(const char* path, const char* mode);
FILE* freopen(const char* path, const char* mode, FILE* oldFile);

constexpr uint64 _IOFBUF = 2;
constexpr uint64 _IOLBUF = 1;
constexpr uint64 _IONBUF = 0;

int setvbuf(FILE* stream, char* buffer, int mode, uint64 size);
void setbuf(FILE* stream, char* buffer);

int64 fread(void* buffer, int64 size, int64 count, FILE* file);
int64 fwrite(const void* buffer, int64 size, int64 count, FILE* file);

int64 fflush(FILE* file);

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int64 ftell(FILE* file);
int64 fseek(FILE* file, int64 offs, int origin);

void clearerr(FILE* stream);

int feof(FILE* stream);
int ferror(FILE* stream);

int64 fputs(const char* str, FILE* file);
int64 puts(const char* str);

void perror(const char* str);