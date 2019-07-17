#include "simpleos_vfs.h"
#include "simpleos_process.h"
#include "simpleos_raw.h"

#include "stdlib.h"

#include "stdio.h"

#define CHECK_ERROR(cond, msg) \
    if(!(cond)) { print(msg "\n"); thread_exit(1); }

int main()
{
    FILE* res = freopen("/dev/tty0", "w", stdout);
    if(res == nullptr)
        exit(123);

    puts("Hello World from Init process\n");

    int64 stdinRead, stdinWrite, stdoutRead, stdoutWrite;
    pipe(&stdinRead, &stdinWrite);
    pipe(&stdoutRead, &stdoutWrite);

    if(fork()) {
        close(stdinRead);
        close(stdoutWrite);

        while(true) {
            char buffer[128];
            int64 len = read(stdoutRead, buffer, sizeof(buffer)-1);
            buffer[len] = 0;
            print(buffer);
        }
    } else {
        copyfd(1, stdoutWrite);

        exec("/boot/Test2.elf");
        thread_exit(1);
    }

    return 0;
}