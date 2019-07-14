#include "stdio.h"

#include "Syscall.h"
#include "string.h"
#include "stdlib.h"
#include "errno.h"

FILE stdinFD = 0;
FILE stdoutFD = 1;
FILE stderrFD = 2;

FILE* stdout = &stdoutFD;
FILE* stdin = &stdinFD;
FILE* stderr = &stderrFD;

int remove(const char* path) {
    int64 error = Syscall::Delete(path);
    if(error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}

int fclose(FILE* file) {
    int64 error = Syscall::Close(*file);
    free(file);

    if(error != 0)
        return EOF;
    return 0;
}
FILE* fopen(const char* path, const char* mode) {
    uint8 perm = 0;
    switch(mode[0]) {
    case 'r': perm = 1; break;
    case 'w': perm = 2; break;
    default: return nullptr;
    }

    int64 desc = Syscall::Open(path, perm);
    if(desc < 0) {
        errno = desc;
        return nullptr;
    }
    
    FILE* res = (FILE*)malloc(sizeof(FILE));
    *res = desc;
    return res;
}
FILE* freopen(const char* path, const char* mode, FILE* oldFile) {
    uint8 perm = 0;
    switch(mode[0]) {
    case 'r': perm = 1; break;
    case 'w': perm = 2; break;
    default: return nullptr;
    }

    int64 error = Syscall::ReplaceFDPath(*oldFile, path, perm);
    if(error != 0) {
        errno = error;
        return nullptr;
    }
    return oldFile;
}

int fpipe(FILE** read, FILE** write) {
    int64 descRead, descWrite;
    Syscall::CreatePipe(&descRead, &descWrite);

    *read = (FILE*)malloc(sizeof(FILE));
    *write = (FILE*)malloc(sizeof(FILE));
    **read = descRead;
    **write = descWrite;

    return 0;
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