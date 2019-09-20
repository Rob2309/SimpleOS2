#include <simpleos_process.h>
#include <stdio.h>
#include <thread.h>
#include <stdlib.h>

int Thread1() {
    while(true) {
        puts("Thread1 alive...\n");
        thread_waitms(1000);
    }
}

int main(int argc, char** argv) {
    puts("Creating Thread1...\n");

    CreateThread(Thread1);

    int64 pid;
    if(pid = fork()) {
        detach(pid);

        thread_waitms(5000);

        exit(0);
    } else {
        while(true) {
            puts("Fork alive...\n");
            thread_waitms(1000);
        }
    }
}