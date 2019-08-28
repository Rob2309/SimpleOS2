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
    int64 tty = open("/dev/tty0", open_mode_write);
    if(tty < 0) {
        print("Failed to open /dev/tty0\n");
        exit(1);
    }

    int64 err = copyfd(1, tty);
    if(err < 0) {
        print("Failed to replace stdout\n");
        exit(1);
    }

    puts("Hello World from Init process\n");

    puts("Executing VFS test\n");
    exec("/boot/TestVFS.elf");

    return 0;
}