#include "stdio.h"

#include "Syscall.h"
#include "string.h"

FILE stdinFD = 0;
FILE stdoutFD = 1;
FILE stderrFD = 2;

FILE* stdout = &stdoutFD;
FILE* stdin = &stdinFD;
FILE* stderr = &stderrFD;

int64 remove(const char* path) {
    return Syscall::Delete(path);
}

static int64 g_FileDescIndex = 0;
static FILE g_FileDescs[200];

int64 fclose(FILE* file) {
    return Syscall::Close(*file);
}
FILE* fopen(const char* path, const char* mode) {
    uint8 perm = 0;
    switch(mode[0]) {
    case 'r': perm = 1; break;
    case 'w': perm = 2; break;
    default: return nullptr;
    }

    int64 desc = Syscall::Open(path, perm);
    if(desc < 0)
        return nullptr;
    
    g_FileDescs[g_FileDescIndex] = desc;
    FILE* res = &g_FileDescs[g_FileDescIndex];
    g_FileDescIndex++;
    return res;
}

int64 fread(void* buffer, int64 size, int64 count, FILE* file) {
    return Syscall::Read(*file, buffer, size * count);
}
int64 fwrite(const void* buffer, int64 size, int64 count, FILE* file) {
    return Syscall::Write(*file, buffer, size * count);
}

int64 fputs(const char* str, FILE* file) {
    int64 len = strlen(str);
    return Syscall::Write(*file, str, len);
}
int64 puts(const char* str) {
    return fputs(str, stdout);
}