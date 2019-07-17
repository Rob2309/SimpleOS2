#include "simpleos_process.h"

#include "stdio.h"
#include "assert.h"
#include "thread.h"

int main()
{
    puts("Starting calculation\n");

    for(uint64 i = 0; i < 500000000; i++) {
        double d = 2.0;
        double e = 3.5;
        int f = (int)(d * e);

        if(f != 7)
            puts("Error\n");
    }

    puts("Finished calculation\n");

    return 0;
}