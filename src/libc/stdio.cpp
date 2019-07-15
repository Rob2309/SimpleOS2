#include "stdio.h"

#include "Syscall.h"
#include "string.h"
#include "stdlib.h"
#include "errno.h"

struct FILE {
    int64 descID;
    bool error;
    bool eof;
};

FILE stdinFile = { 0, false, false };
FILE stdoutFile = { 1, false, false };
FILE stderrFile = { 2, false, false };

FILE* stdout = &stdoutFile;
FILE* stdin = &stdinFile;
FILE* stderr = &stderrFile;

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
    int64 error = Syscall::Close(file->descID);
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
    res->descID = desc;
    res->eof = false;
    res->error = false;
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

    int64 error = Syscall::ReplaceFDPath(oldFile->descID, path, req);
    if(error != 0) {
        errno = error;
        free(oldFile);
        return nullptr;
    }

    oldFile->eof = false;
    oldFile->error = false;
    return oldFile;
}

int64 fread(void* buffer, int64 size, int64 count, FILE* file) {
    return Syscall::Read(file->descID, buffer, size * count);
}
int64 fwrite(const void* buffer, int64 size, int64 count, FILE* file) {
    return Syscall::Write(file->descID, buffer, size * count);
}

int64 fputs(const char* str, FILE* file) {
    int64 len = strlen(str);
    return Syscall::Write(file->descID, str, len);
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