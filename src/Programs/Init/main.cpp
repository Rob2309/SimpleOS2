#include "simpleos_vfs.h"
#include "simpleos_process.h"
#include "simpleos_raw.h"
#include "syscall.h"

#include "stdlib.h"

#include "stdio.h"

#define CHECK_ERROR(cond, msg) \
    if(!(cond)) { print(msg "\n"); thread_exit(1); }

int main(int argc, char** argv)
{
    int64 outfd = open("/dev/tty0", open_mode_write);
    if(outfd < 0) {
        print("Failed to open /dev/tty0\n");
        exit(1);
    }

    int64 err = copyfd(stdoutfd, outfd);
    if(err < 0) {
        print("Failed to replace stdout\n");
        exit(1);
    }

    int64 infd = open("/dev/tty0", open_mode_read);
    if(infd < 0) {
        puts("Failed to open /dev/keyboard\n");
        exit(1);
    }
    err = copyfd(stdinfd, infd);
    if(err < 0) {
        puts("Failed to replace stdin\n");
        exit(1);
    }

    puts("Hello World from Init process\n");

    puts("Starting shell\n");
    exec("/boot/Shell.elf", 0, nullptr);

    return 0;
}