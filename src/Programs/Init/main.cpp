#include "simpleos_vfs.h"
#include "simpleos_process.h"
#include "simpleos_raw.h"
#include "syscall.h"

#include "stdlib.h"

#include "stdio.h"

#define CHECK_ERROR(cond, msg) \
    if(!(cond)) { print(msg "\n"); thread_exit(1); }

int main()
{
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
        copyfd(0, stdinRead);
        copyfd(1, stdoutWrite);

        //exec("/boot/TestVFS.elf");
        thread_exit(1);
    }

    return 0;
}