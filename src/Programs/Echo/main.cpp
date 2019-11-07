#include <simpleos_inout.h>

int main(int argc, char** argv) {
    for(int i = 1; i < argc; i++) {
        puts(argv[i]);
        puts(" ");
    }

    puts("\n");
    return 0;
}