#include "simpleos_vfs.h"
#include "simpleos_process.h"
#include "simpleos_raw.h"

#include "stdio.h"

#define CHECK_ERROR(func, msg) \
    if(func < 0) { print(msg "\n"); thread_exit(1); }

extern "C" void main()
{
    CHECK_ERROR(create_folder("/dev"), "Failed to create /dev folder");
    CHECK_ERROR(create_dev("/dev/zero", 0, 0), "Failed to create /dev/zero");
    CHECK_ERROR(create_dev("/dev/ram0", 1, 0), "Failed to create /dev/ram0");

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
}