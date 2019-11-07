#include "simpleos_vfs.h"
#include "simpleos_process.h"
#include "simpleos_raw.h"
#include "simpleos_inout.h"

#define CHECK_ERROR(cond, msg) \
    if(!(cond)) { print(msg "\n"); thread_exit(1); }

int main(int argc, char** argv)
{
    int64 outfd = open("/dev/tty0", open_mode_write);
    if(outfd < 0) {
        print("Failed to open /dev/tty0\n");
        thread_exit(1);
    }

    int64 err = copyfd(stdoutfd, outfd);
    if(err < 0) {
        print("Failed to replace stdout\n");
        thread_exit(1);
    }

    int64 infd = open("/dev/tty0", open_mode_read);
    if(infd < 0) {
        puts("Failed to open /dev/keyboard\n");
        thread_exit(1);
    }
    err = copyfd(stdinfd, infd);
    if(err < 0) {
        puts("Failed to replace stdin\n");
        thread_exit(1);
    }

    puts("Hello World from Init process\n");

    puts("Starting login\n");
    exec("/boot/Login.elf", 0, nullptr);

    return 0;
}