#include "string.h"

int kstrcmp(const char* a, const char* b)
{
    int i = 0;
    while(a[i] == b[i]) {
        if(a[i] == '\0')
            return 0;
        i++;
    }
    return 1;
}

int kstrcmp(const char* a, int aStart, int aEnd, const char* b)
{
    int length = aEnd - aStart;
    for(int i = 0; i < length; i++) {
        if(a[i + aStart] != b[i])
            return 1;
    }
    return 0;
}

int kstrlen(const char* a)
{
    int l = 0;
    while(a[l] != '\0')
        l++;
    return l;
}