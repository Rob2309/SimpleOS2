#include <simpleos_vfs.h>
#include <simpleos_inout.h>
#include <simpleos_errno.h>

int main(int argc, char** argv) {
    if(argc != 2) {
        puts("Usage: cat <file>\n");
        return 0;
    }

    int64 fd = open(argv[1], open_mode_read);
    if(fd < 0) {
        puts(ErrorToString(fd));
        puts("\n");
        return fd;
    }

    char buffer[128];
    while(true) {
        int64 count = read(fd, buffer, sizeof(buffer));
        if(count < 0)
            return count;
        if(count == 0)
            break;

        write(stdoutfd, buffer, count);
    }

    puts("\n");

    close(fd);
    return 0;
}