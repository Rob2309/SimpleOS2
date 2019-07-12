#include "stdio.h"

#include "Syscall.h"
#include "string.h"

FILE stdinFD = 0;
FILE stdoutFD = 1;
FILE stderrFD = 2;

FILE* stdout = &stdoutFD;
FILE* stdin = &stdinFD;
FILE* stderr = &stderrFD;

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