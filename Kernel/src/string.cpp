#include "string.h"

int strcmp(const char* a, const char* b)
{
    int i = 0;
    while(a[i] == b[i]) {
        if(a[i] == '\0')
            return 0;
        i++;
    }
    return 1;
}