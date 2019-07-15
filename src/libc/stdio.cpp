#include "stdio.h"

#include "Syscall.h"
#include "string.h"
#include "stdlib.h"
#include "errno.h"

enum BufferMode {
    BUFFERMODE_NONE,
    BUFFERMODE_LINE,
    BUFFERMODE_FULL,
};

struct FILE {
    int64 descID;
    bool error;
    bool eof;

    BufferMode bufferMode;
    int64 bufferCapacity;
    int64 bufferFillSize;
    int64 bufferPos;
    char* buffer;
};

constexpr uint64 DefaultBufferSize = 4096;

FILE stdinFile = { 0, false, false, BUFFERMODE_NONE, 0, 0, 0, nullptr };
FILE stdoutFile = { 1, false, false, BUFFERMODE_NONE, 0, 0, 0, nullptr };
FILE stderrFile = { 2, false, false, BUFFERMODE_NONE, 0, 0, 0, nullptr };

FILE* stdout = &stdoutFile;
FILE* stdin = &stdinFile;
FILE* stderr = &stderrFile;

static FILE* CreateFullyBuffered(uint64 bufferSize) {
    FILE* res = (FILE*)malloc(sizeof(FILE));
    res->descID = 0;
    res->error = false;
    res->eof = false;
    res->bufferFillSize = 0;
    res->bufferCapacity = bufferSize;
    res->bufferPos = 0;
    res->buffer = (char*)malloc(bufferSize);
    res->bufferMode = BUFFERMODE_FULL;
    return res;
}

static FILE* CreateUnbuffered() {
    FILE* res = (FILE*)malloc(sizeof(FILE));
    res->descID = 0;
    res->error = false;
    res->eof = false;
    res->bufferCapacity = 0;
    res->bufferFillSize = 0;
    res->bufferPos = 0;
    res->buffer = nullptr;
    res->bufferMode = BUFFERMODE_NONE;
    return res;
}

static void FreeFile(FILE* file) {
    if(file->buffer != nullptr)
        free(file->buffer);

    free(file);
}

int remove(const char* path) {
    int64 error = delete_file(path);
    if(error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}

int fclose(FILE* file) {
    int64 error = close(file->descID);
    free(file);

    if(error != 0)
        return EOF;
    return 0;
}
FILE* fopen(const char* path, const char* mode) {
    uint64 req = 0;
    switch(mode[0]) {
    case 'r': req = open_mode_read; break;
    case 'w': req = open_mode_create | open_mode_clear | open_mode_write; break;
    default: return nullptr;
    }
    int index = 1;
    while(mode[index] != '\0') {
        switch (mode[index])
        {
        case '+': req |= open_mode_read | open_mode_write; break;
        case 'x': req |= open_mode_failexist; break;
        case 'b': break;
        default: return nullptr; break;
        }
        index++;
    }

    int64 desc = open(path, req);
    if(desc < 0) {
        errno = desc;
        return nullptr;
    }
    
    FILE* res = CreateFullyBuffered(DefaultBufferSize);
    res->descID = desc;
    return res;
}
FILE* freopen(const char* path, const char* mode, FILE* oldFile) {
    uint64 req = 0;
    switch(mode[0]) {
    case 'r': req = open_mode_read; break;
    case 'w': req = open_mode_create | open_mode_clear | open_mode_write; break;
    default: return nullptr;
    }
    int index = 1;
    while(mode[index] != '\0') {
        switch (mode[index])
        {
        case '+': req |= open_mode_read | open_mode_write; break;
        case 'x': req |= open_mode_failexist; break;
        case 'b': break;
        default: return nullptr; break;
        }
        index++;
    }

    int64 error = reopenfd(oldFile->descID, path, req);
    if(error != 0) {
        errno = error;
        FreeFile(oldFile);
        return nullptr;
    }

    oldFile->bufferPos = 0;
    oldFile->bufferFillSize = 0;
    oldFile->eof = false;
    oldFile->error = false;
    return oldFile;
}

int setvbuf(FILE* stream, char* buffer, int mode, uint64 size) {
    if(mode == _IONBUF) {
        free(stream->buffer);
        stream->buffer = nullptr;
        stream->bufferCapacity = 0;
        stream->bufferFillSize = 0;
        stream->bufferPos = 0;
        stream->bufferMode = BUFFERMODE_NONE;
        return 0;
    } else {
        free(stream->buffer);

        if(buffer == nullptr)
            buffer = (char*)malloc(size);

        stream->buffer = buffer;
        stream->bufferCapacity = size;
        stream->bufferPos = 0;
        stream->bufferFillSize = 0;
        stream->bufferMode = (BufferMode)mode;

        return 0;
    }
}
void setbuf(FILE* stream, char* buffer) {
    if(buffer == nullptr) {
        free(stream->buffer);
        stream->buffer = nullptr;
        stream->bufferMode = BUFFERMODE_NONE;
        stream->bufferCapacity = 0;
        stream->bufferFillSize = 0;
        stream->bufferPos = 0;
    } else {
        free(stream->buffer);
        stream->buffer = buffer;
        stream->bufferFillSize = 0;
        stream->bufferPos = 0;
    }
}

static bool fread_fullbuffer_ensurebuffer(FILE* file) {
    int64 rem = file->bufferFillSize - file->bufferPos;
    if(rem == 0) {
        file->bufferPos = 0;
        int64 count = read(file->descID, file->buffer, file->bufferCapacity);
        if(count == 0) {
            file->eof = true;
            file->bufferFillSize = 0;
            return false;
        } else if(count < 0) {
            file->error = true;
            file->bufferFillSize = 0;
            return false;
        } else {
            file->bufferFillSize = count;
            return true;
        }
    }
    return true;
}

static int64 fread_fullbuffer(void* buffer, int64 size, FILE* file) {
    int64 numRead = 0;
    while(numRead < size) {
        if(!fread_fullbuffer_ensurebuffer(file))
            return numRead;

        int64 rem = file->bufferFillSize;
        if(rem > size - numRead)
            rem = size - numRead;

        for(int64 i = 0; i < rem; i++)
            ((char*)buffer)[numRead + i] = file->buffer[i];

        file->bufferPos += rem;
        numRead += rem;
    }

    return numRead;
}

static int64 fread_nobuffer(void* buffer, int64 size, FILE* file) {
    int64 count = read(file->descID, buffer, size);
    if(count < 0) {
        errno = count;
        file->error = true;
        return 0;
    } else if(count < size) {
        file->eof = true;
        return count;
    } else {
        return count;
    }
}

int64 fread(void* buffer, int64 size, int64 count, FILE* file) {
    if(size == 0 || count == 0)
        return 0;

    if(file->bufferMode == BUFFERMODE_FULL) {
        return fread_fullbuffer(buffer, size * count, file);
    } else {
        return fread_nobuffer(buffer, size * count, file);
    }
}

static bool flush_buffer(FILE* file) {
    if(file->bufferMode == BUFFERMODE_NONE)
        return true;

    while(file->bufferPos < file->bufferFillSize) {
        int64 count = write(file->descID, file->buffer + file->bufferPos, file->bufferFillSize - file->bufferPos);
        if(count <= 0) {
            errno = count;
            file->error = true;
            return false;
        } else {
            file->bufferPos += count;
        }
    }

    file->bufferPos = 0;
    file->bufferFillSize = 0;
    return true;
}

static bool fwrite_fullbuffer_ensurebuffer(FILE* file) {
    int cap = file->bufferCapacity - file->bufferFillSize;
    if(cap == 0)
        return flush_buffer(file);

    return true;
}

static int64 fwrite_fullbuffer(const void* buffer, int64 size, FILE* file) {
    int64 numWritten = 0;
    while(numWritten < size) {
        fwrite_fullbuffer_ensurebuffer(file);

        int64 rem = file->bufferCapacity - file->bufferPos;
    }
}

static int64 fwrite_nobuffer(const void* buffer, int64 size, FILE* file) {
    int64 count = write(file->descID, buffer, size);
    if(count < 0) {
        errno = count;
        file->error = true;
        return 0;
    } else if(count < size) {
        file->eof = true;
        return count;
    } else {
        return count;
    }
}

int64 fwrite(const void* buffer, int64 size, int64 count, FILE* file) {
    if(file->bufferMode == BUFFERMODE_FULL) {
        return fwrite_fullbuffer(buffer, size * count, file);
    } else {
        return fwrite_nobuffer(buffer, size * count, file);
    }
}

int64 fputs(const char* str, FILE* file) {
    int64 len = strlen(str);
    return write(file->descID, str, len);
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