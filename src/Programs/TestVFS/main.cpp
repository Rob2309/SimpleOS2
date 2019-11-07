#include "simpleos_process.h"
#include <simpleos_vfs.h>
#include <simpleos_inout.h>
#include <simpleos_string.h>
#include <simpleos_kill.h>

// Test1: create folder, link to folder, create file using symlink, open file using original folder
static void Test1() {
    puts("Creating /usr\n");
    if(create_folder("/usr") < 0) {
        puts("Failed...");
        thread_exit(1);
    }

    puts("Creating symlink /usr2 -> /usr\n");
    if(create_symlink("/usr2", "/usr") < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Creating /usr2/test.txt\n");
    if(create_file("/usr2/test.txt") < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Opening /usr/test.txt\n");
    int64 fd = open("/usr/test.txt", open_mode_read);
    if(fd < 0) {
        puts("Failed...\n");
        thread_exit(1);
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
        thread_exit(1);
    }

    puts("Trying to open /usr/test.txt\n");
    int64 fd = open("/usr/test.txt", open_mode_read);
    if(fd >= 0) {
        puts("File still exists...\n");
        close(fd);
        thread_exit(1);
    }

    puts("Test2 successful\n");
}

// Test3: delete /usr2, try to create /usr/test.txt
static void Test3() {
    puts("Deleting /usr2\n");
    if(delete_file("/usr2") < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Creating /usr/test.txt\n");
    if(create_file("/usr/test.txt") < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Test3 successful\n");
}

// Test4: create /usr2 -> /usr, try to create /usr2/test.txt
static void Test4() {
    puts("Creating /usr2 -> /usr\n");
    if(create_symlink("/usr2", "/usr") < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Test 4 successful\n");
}

// Test6: open /usr2/test.txt
static void Test6() {
    puts("Opening /usr2/test.txt\n");
    int64 fd = open("/usr2/test.txt", open_mode_read);
    if(fd < 0) {
        puts("Failed...\n");
        thread_exit(1);
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
        thread_exit(1);
    }

    puts("Creating /etc/test.txt\n");
    if(create_file("/etc/test.txt") < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Creating hardlink /etc/test2.txt -> /etc/test.txt\n");
    if(create_hardlink("/etc/test2.txt", "/etc/test.txt") < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Test 7 successful\n");
}

// Test8: open /etc/test2.txt, delete /etc/test2.txt, Write "Hello World\n" to fd, close fd, open /etc/test.txt, read fd, print content
static void Test8() {
    puts("Opening /etc/test2.txt\n");
    int64 fd = open("/etc/test2.txt", open_mode_write);
    if(fd < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Deleting /etc/test2.txt\n");
    if(delete_file("/etc/test2.txt") < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    const char* msg = "Hello World\n";

    puts("Writing to /etc/test2.txt\n");
    int64 count = write(fd, msg, strlen(msg));
    if(count < strlen(msg)) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Closing /etc/test2.txt\n");
    if(close(fd) < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Opening /etc/test.txt\n");
    fd = open("/etc/test.txt", open_mode_read);
    if(fd < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    char buffer[128] = { 0 };
    count = read(fd, buffer, 127);
    if(count != strlen(msg)) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Closing /etc/test.txt\n");
    if(close(fd) < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Content: ");
    puts(buffer);

    puts("Test 8 probably successful\n");
}

// Pass identical path to create_hardlink
static void Test9() {
    puts("Creating hardlink /etc/test.txt -> /etc/test.txt\n");
    if(create_hardlink("/etc/test.txt", "/etc/test.txt") == 0) {
        puts("Returned OK even though operation is impossible\n");
        thread_exit(1);
    }

    puts("Creating hardlink /etc/test3.txt -> /etc/test3.txt\n");
    if(create_hardlink("/etc/test3.txt", "/etc/test3.txt") == 0) {
        puts("Returned OK even though operation is impossible\n");
        thread_exit(1);
    }

    puts("Test 9 successful\n");
}

// Test10: Create /tmp, mount tempfs, unmount tempfs
static void Test10() {
    puts("Creating /tmp\n");
    if(create_folder("/tmp") < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Mounting tmpfs to /tmp\n");
    if(mount("/tmp", "tempfs") < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Unmounting /tmp\n");
    if(unmount("/tmp") < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Test 10 successful\n");
}

// Test11: mount tempfs to /tmp, create /tmp/test.txt, open /tmp/test.txt, try unmounting /tmp, close /tmp/test.txt, try again
static void Test11() {
    puts("Mounting tmpfs to /tmp\n");
    if(mount("/tmp", "tempfs") < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Creating and opening /tmp/test.txt\n");
    int fd = open("/tmp/test.txt", open_mode_write | open_mode_create);
    if(fd < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Trying to unmount /tmp\n");
    if(unmount("/tmp") == 0) {
        puts("Unmount returned OK even though a file was still open\n");
        thread_exit(1);
    }

    puts("Closing /tmp/test.txt");
    if(close(fd) < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Trying to unmount /tmp\n");
    if(unmount("/tmp") < 0) {
        puts("Failed...\n");
        thread_exit(1);
    }

    puts("Test 11 successful\n");
}

static void Kill() {
    puts("Kill handler called\n");
}

int main(int argc, char** argv)
{
    Test1();
    Test2();
    Test3();
    Test4();
    Test6();
    Test7();
    Test8();
    Test9();
    Test10();
    Test11();

    puts("All tests successful!\n");
    return 0;
}