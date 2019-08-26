#include "simpleos_process.h"

#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include "thread.h"

// Test1: create folder, link to folder, create file using symlink, open file using original folder
static void Test1() {
    puts("Creating /usr\n");
    if(create_folder("/usr") < 0) {
        puts("Failed...");
        exit(1);
    }

    puts("Creating symlink /usr2 -> /usr\n");
    if(create_symlink("/usr2", "/usr") < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Creating /usr2/test.txt\n");
    if(create_file("/usr2/test.txt") < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Opening /usr/test.txt\n");
    int64 fd = open("/usr/test.txt", open_mode_read);
    if(fd < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Test1 successful\n");
}

// Test2: delete /usr2/test.txt, try to open /usr/test.txt
static void Test2() {
    puts("Deleting /usr2/test.txt\n");
    if(delete_file("/usr2/test.txt") < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Trying to open /usr/test.txt\n");
    int64 fd = open("/usr/test.txt", open_mode_read);
    if(fd >= 0) {
        puts("File still exists...\n");
        exit(1);
    }

    puts("Test2 successful\n");
}

// Test3: delete /usr2, try to create /usr/test.txt
static void Test3() {
    puts("Deleting /usr2\n");
    if(delete_file("/usr2") < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Creating /usr/test.txt\n");
    if(create_file("/usr/test.txt") == 0) {
        puts("/usr still exists...\n");
        exit(1);
    }

    puts("Test3 successful\n");
}

// Test4: create /usr, try to create /usr2/test.txt
static void Test4() {
    puts("Creating /usr\n");
    if(create_folder("/usr") < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Creating /usr2/test.txt\n");
    if(create_file("/usr2/test.txt") < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Test 4 successful\n");
}

int main()
{
    Test1();
    Test2();
    Test3();
    Test4();

    puts("All tests successful!\n");
    return 0;
}