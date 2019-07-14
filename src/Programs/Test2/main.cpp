#include "Syscall.h"

#include "stdio.h"
#include "assert.h"

#include "thread.h"

__thread int t_Test = 15;

static int Thread1() {
    assert(t_Test == 15);
    return 0;
}

extern "C" void main()
{
    t_Test = 17;

    CreateThread(Thread1);

    puts("Hello World from Test program\n");
    puts("This is another message\n");

    Syscall::Exit(0);
}