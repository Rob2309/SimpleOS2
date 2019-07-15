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

constexpr uint64 OpenMode_Read = 0x1;
constexpr uint64 OpenMode_Write = 0x2;
constexpr uint64 OpenMode_Create = 0x100;
constexpr uint64 OpenMode_Clear = 0x200;
constexpr uint64 OpenMode_FailIfExist = 0x400;

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
    uint64 req = 0;
    switch(mode[0]) {
    case 'r': req = OpenMode_Read; break;
    case 'w': req = OpenMode_Create | OpenMode_Clear | OpenMode_Write; break;
    default: return nullptr;
    }
    int index = 1;
    while(mode[index] != '\0') {
        switch (mode[index])
        {
        case '+': req |= OpenMode_Read | OpenMode_Write; break;
        case 'x': req |= OpenMode_FailIfExist; break;
        case 'b': break;
        default: return nullptr; break;
        }
        index++;
    }

    int64 desc = Syscall::Open(path, req);
    if(desc < 0) {
        errno = desc;
        return nullptr;
    }
    
    FILE* res = (FILE*)malloc(sizeof(FILE));
    *res = desc;
    return res;
}
FILE* freopen(const char* path, const char* mode, FILE* oldFile) {
    uint64 req = 0;
    switch(mode[0]) {
    case 'r': req = OpenMode_Read; break;
    case 'w': req = OpenMode_Create | OpenMode_Clear | OpenMode_Write; break;
    default: return nullptr;
    }
    int index = 1;
    while(mode[index] != '\0') {
        switch (mode[index])
        {
        case '+': req |= OpenMode_Read | OpenMode_Write; break;
        case 'x': req |= OpenMode_FailIfExist; break;
        case 'b': break;
        default: return nullptr; break;
        }
        index++;
    }

    int64 error = Syscall::ReplaceFDPath(*oldFile, path, req);
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

void perror(const char* str) {
    fputs(str, stderr);
    
    const char* err;
    switch(errno) {
    case EFILENOTFOUND: err = "File not found\n"; break;
    case EPERMISSIONDENIED: err = "Permission denied\n"; break;
    case EINVALIDFD: err = "Invalid file descriptor\n"; break;
    case EINVALIDBUFFER: err = "Invalid buffer\n"; break;
    case EINVALIDPATH: err = "Ill-formed path\n"; break;
    case EINVALIDFS: err = "Invalid filesystem ID\n"; break;
    case EINVALIDDEV: err = "Invalid device\n"; break;
    case EFILEEXISTS: err = "File exists\n"; break;
    default: err = "unknown error\n"; break;
    }

    fputs(err, stderr);
}