#include "stdio.h"

#include "syscall.h"
#include "string.h"
#include "stdlib.h"
#include "errno.h"
#include "string.h"

#include "internal/nlist.h"

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
    int64 bufferSize;
    int64 bufferReadPos;
    int64 bufferReadSize;
    int64 bufferWritePos;
    char* buffer;

    int64 ftellPos;

    FILE* next;
    FILE* prev;
};

FILE stdinFile = { 0, false, false, BUFFERMODE_NONE, 0, 0, 0, 0, nullptr };
FILE stdoutFile = { 1, false, false, BUFFERMODE_NONE, 0, 0, 0, 0, nullptr };
FILE stderrFile = { 2, false, false, BUFFERMODE_NONE, 0, 0, 0, 0, nullptr };

FILE* stdout = &stdoutFile;
FILE* stdin = &stdinFile;
FILE* stderr = &stderrFile;

static nlist<FILE> g_OpenFiles;

static FILE* CreateFullyBuffered(uint64 bufferSize) {
    FILE* file = (FILE*)malloc(sizeof(FILE));
    file->descID = 0;
    file->error = false;
    file->eof = false;
    file->bufferMode = BUFFERMODE_FULL;
    file->bufferSize = bufferSize;
    file->bufferReadPos = 0;
    file->bufferWritePos = 0;
    file->bufferReadSize = 0;
    file->buffer = (char*)malloc(bufferSize);
    file->ftellPos = 0;
}

static FILE* CreateUnbuffered() {
    FILE* file = (FILE*)malloc(sizeof(FILE));
    file->descID = 0;
    file->error = false;
    file->eof = false;
    file->bufferMode = BUFFERMODE_NONE;
    file->bufferSize = 0;
    file->bufferReadPos = 0;
    file->bufferWritePos = 0;
    file->bufferReadSize = 0;
    file->buffer = nullptr;
    file->ftellPos = 0;
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
    fflush(file);

    int64 res = close(file->descID);
    
    g_OpenFiles.erase(file);
    FreeFile(file);

    return res;
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
    
    FILE* res = CreateFullyBuffered(BUFSIZ);
    res->descID = desc;
    g_OpenFiles.push_back(res);
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

    int64 newFD = open(path, req);
    if(newFD < 0) {
        errno = newFD;
        fclose(oldFile);
        return nullptr;
    }

    int64 error = copyfd(oldFile->descID, newFD);
    if(error < 0) {
        errno = error;
        fclose(oldFile);
        return nullptr;
    }

    return oldFile;
}

int setvbuf(FILE* stream, char* buffer, int mode, uint64 size) {
    if(mode == _IONBUF) {
        free(stream->buffer);
        stream->bufferMode = BUFFERMODE_NONE;
        stream->buffer = nullptr;
        stream->bufferSize = 0;
        return 0;
    } else {
        free(stream->buffer);

        if(buffer == nullptr)
            buffer = (char*)malloc(size);

        stream->buffer = buffer;
        stream->bufferMode = BUFFERMODE_FULL;
        stream->bufferSize = size;

        return 0;
    }
}
void setbuf(FILE* stream, char* buffer) {
    if(buffer == nullptr) {
        free(stream->buffer);
        stream->buffer = nullptr;
        stream->bufferMode = BUFFERMODE_NONE;
        stream->bufferSize = 0;
    } else {
        free(stream->buffer);
        stream->buffer = buffer;
        stream->bufferSize = BUFSIZ;
        stream->bufferMode = BUFFERMODE_FULL;
    }
}

static bool flush_buffer(FILE* file) {
    int64 numWritten = 0;
    while(numWritten < file->bufferWritePos) {
        int64 count = write(file->descID, (char*)file->buffer + numWritten, file->bufferWritePos - numWritten);
        if(count <= 0) {
            errno = count;
            file->error = true;
            return false;
        } else {
            numWritten += count;
        }
    }

    file->bufferWritePos = 0;
    return true;
}

static int64 _fread_nobuffer(void* buffer, int64 size, FILE* file) {
    int64 numRead = 0;
    while(numRead < size) {
        int64 count = read(file->descID, (char*)buffer + numRead, size - numRead);
        if(count < 0) {
            errno = count;
            file->error = true;
            return numRead;
        } else if(count == 0) {
            file->eof = true;
            return numRead;
        } else {
            numRead += count;
        }
    }
    return numRead;
}

static bool _fread_fullbuffer_fill(FILE* file) {
    int64 cap = file->bufferReadSize - file->bufferReadPos;
    if(cap != 0)
        return true;

    file->bufferReadSize = 0;
    file->bufferReadPos = 0;

    int64 count = read(file->descID, file->buffer, file->bufferSize);
    if(count < 0) {
        errno = count;
        file->error = true;
        return false;
    } else if(count == 0) {
        file->eof = true;
        return false;
    } else {
        file->bufferReadPos = 0;
        file->bufferReadSize = count;
        return true;
    }
}

static int64 _fread_fullbuffer(void* buffer, int64 size, FILE* file) {
    int64 numRead = 0;
    while(numRead < size) {
        if(!_fread_fullbuffer_fill(file))
            return numRead;
        
        int64 cap = file->bufferReadSize - file->bufferReadPos;
        if(cap > size - numRead)
            cap = size - numRead;
        
        memcpy((char*)buffer + numRead, (char*)file->buffer + file->bufferReadPos, cap);

        numRead += cap;
        file->bufferReadPos += cap;
    }
    return numRead;
}

int64 fread(void* buffer, int64 size, int64 count, FILE* file) {
    if(file->bufferWritePos != 0)
        return 0;

    int64 res;

    if(file->bufferMode == BUFFERMODE_NONE)
        res = _fread_nobuffer(buffer, size * count, file);
    else if(file->bufferMode == BUFFERMODE_LINE)
        res = 0;
    else 
        res = _fread_fullbuffer(buffer, size * count, file);

    if(res > 0)
        file->ftellPos += res;

    return res;
}

static int64 _fwrite_nobuffer(const void* buffer, int64 size, FILE* file) {
    int64 numWritten = 0;
    while(numWritten < size) {
        int64 count = write(file->descID, (char*)buffer + numWritten, size - numWritten);

        if(count <= 0) {
            errno = count;
            file->error = true;
            return numWritten;
        } else {
            numWritten += count;
        }
    }
}

static bool _fwrite_fullbuffer_flush(FILE* file) {
    int64 cap = file->bufferSize - file->bufferWritePos;
    if(cap != 0)
        return true;

    return flush_buffer(file);
}

static int64 _fwrite_fullbuffer(const void* buffer, int64 size, FILE* file) {
    int64 numWritten = 0;
    while(numWritten < size) {
        if(!_fwrite_fullbuffer_flush(file))
            return numWritten;

        int64 cap = file->bufferSize - file->bufferWritePos;
        if(cap > size - numWritten)
            cap = size - numWritten;

        memcpy((char*)file->buffer + file->bufferWritePos, (char*)buffer + numWritten, cap);

        numWritten += cap;
        file->bufferWritePos += cap;
    }
    return numWritten;
}

int64 fwrite(const void* buffer, int64 size, int64 count, FILE* file) {
    if(file->bufferReadPos != 0 || file->bufferReadSize != 0)
        return 0;

    int64 res;

    if(file->bufferMode == BUFFERMODE_NONE)
        res = _fwrite_nobuffer(buffer, size * count, file);
    else if(file->bufferMode == BUFFERMODE_LINE)
        res = 0;
    else
        res = _fwrite_fullbuffer(buffer, size * count, file);

    if(res > 0)
        file->ftellPos += res;

    return res;   
}

static int64 fflush_all() {
    for(FILE* f : g_OpenFiles) {
        fflush(f);
    }
    return 0;
}

int64 fflush(FILE* file) {
    if(file->bufferWritePos == 0)
        return 0;

    if(!flush_buffer(file))
        return EOF;
    return 0;
}

int64 ftell(FILE* file) {
    return file->ftellPos;
}
int64 fseek(FILE* file, int64 offs, int origin) {
    if(fflush(file) != 0)
        return EOF;

    file->bufferReadPos = 0;
    file->bufferReadSize = 0;

    int64 newOffs = 0;
    if(origin == SEEK_SET)
        newOffs = offs;
    else if(origin == SEEK_CUR)
        newOffs = file->ftellPos + offs;
    else
        newOffs = 0;

    int64 error = seekfd(file->descID, seek_mode_set, newOffs);
    if(error != 0) {
        errno = error;
        file->error = true;
        return EOF;
    }
    
    file->ftellPos = newOffs;
    return 0;
}

void clearerr(FILE* stream) {
    stream->eof = false;
    stream->error = false;
}

int feof(FILE* stream) {
    return stream->eof;
}
int ferror(FILE* stream) {
    return stream->error;
}

int64 fputs(const char* str, FILE* file) {
    int64 len = strlen(str);
    return fwrite(str, 1, len, file);
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
    case ESEEKOOB: err = "Seek offset out of bounds\n"; break;
    case ESYMLINK: err = "Encountered Symlink\n"; break;
    default: err = "unknown error\n"; break;
    }

    fputs(err, stderr);
}


void stdio_cleanup() {
    for(FILE* f : g_OpenFiles) {
        fclose(f);
    }
}