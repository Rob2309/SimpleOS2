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
    int64 tty = open("/dev/tty0", open_mode_write);
    if(tty < 0) {
        print("Failed to open /dev/tty0\n");
        exit(1);
    }

    int64 err = copyfd(stdoutfd, tty);
    if(err < 0) {
        print("Failed to replace stdout\n");
        exit(1);
    }

    puts("Hello World from Init process\n");

    int64 fd = open("/dev/keyboard", open_mode_read);
    if(fd < 0) {
        puts("Failed to open /dev/keyboard\n");
        exit(1);
    }
    err = copyfd(stdinfd, fd);
    if(err < 0) {
        puts("Failed to replace stdin\n");
        exit(1);
    }

    puts("Starting shell\n");
    exec("/boot/Shell.elf");

    return 0;
}