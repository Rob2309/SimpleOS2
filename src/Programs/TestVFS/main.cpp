#include "simpleos_process.h"

#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include "thread.h"

#include "string.h"

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
    } else {
        close(fd);
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
        close(fd);
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

// Test5: try mounting tempfs to /usr2, delete /usr2/test.txt, try again
static void Test5() {
    puts("Trying to mount to /usr2\n");
    if(mount("/usr2", "tempfs") == 0) {
        puts("Mount succeeded even though folder is not empty\n");
        exit(1);
    }

    puts("Deleting /usr2/test.txt\n");
    if(delete_file("/usr2/test.txt") < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Mounting to /usr2\n");
    if(mount("/usr2", "tempfs") < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Test 5 successful\n");
}

// Test6: create /usr/test.txt, open /usr2/test.txt
static void Test6() {
    puts("Creating /usr/test.txt\n");
    if(create_file("/usr/test.txt") < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Opening /usr2/test.txt\n");
    int64 fd = open("/usr2/test.txt", open_mode_read);
    if(fd < 0) {
        puts("Failed...\n");
        exit(1);
    } else {
        close(fd);
    }

    puts("Test 6 successful\n");
}

// Test7: Create /etc, Create /etc/test.txt, Create hardlink /etc/test2.txt -> /etc/test.txt
static void Test7() {
    puts("Creating /etc\n");
    if(create_folder("/etc") < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Creating /etc/test.txt\n");
    if(create_file("/etc/test.txt") < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Creating hardlink /etc/test2.txt -> /etc/test.txt\n");
    if(create_hardlink("/etc/test2.txt", "/etc/test.txt") < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Test 7 successful\n");
}

// Test8: open /etc/test2.txt, delete /etc/test2.txt, Write "Hello World\n" to fd, close fd, open /etc/test.txt, read fd, print content
static void Test8() {
    puts("Opening /etc/test2.txt\n");
    int64 fd = open("/etc/test2.txt", open_mode_write);
    if(fd < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Deleting /etc/test2.txt\n");
    if(delete_file("/etc/test2.txt") < 0) {
        puts("Failed...\n");
        exit(1);
    }

    const char* msg = "Hello World\n";

    puts("Writing to /etc/test2.txt\n");
    int64 count = write(fd, msg, strlen(msg));
    if(count < strlen(msg)) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Closing /etc/test2.txt\n");
    if(close(fd) < 0) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Opening /etc/test.txt\n");
    fd = open("/etc/test.txt", open_mode_read);
    if(fd < 0) {
        puts("Failed...\n");
        exit(1);
    }

    char buffer[128] = { 0 };
    count = read(fd, buffer, 127);
    if(count != strlen(msg)) {
        puts("Failed...\n");
        exit(1);
    }

    puts("Content: ");
    puts(buffer);

    puts("Test 8 probably successful\n");
}

int main()
{
    Test1();
    Test2();
    Test3();
    Test4();
    Test5();
    Test6();
    Test7();
    Test8();

    puts("All tests successful!\n");
    return 0;
}