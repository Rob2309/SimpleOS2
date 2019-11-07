#include <simpleos_vfs.h>
#include <simpleos_inout.h>

int main(int argc, char** argv) {
    if(argc < 2) {
        puts("Usage: cat <file>\n");
        return 0;
    }

    int64 fd = open(argv[1], open_mode_read);
    if(fd < 0) {
        puts("File not found: ");
        puts(argv[1]);
        puts("\n");
        return 1;
    }

    char buffer[128];
    while(true) {
        int64 count = read(fd, buffer, sizeof(buffer));
        if(count < 0)
            return 1;
        if(count == 0)
            break;

        write(stdoutfd, buffer, count);
    }

    close(fd);
    return 0;
}