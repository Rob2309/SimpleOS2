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

int strcmp(const char* a, int aStart, int aEnd, const char* b)
{
    int length = aEnd - aStart;
    for(int i = 0; i < length; i++) {
        if(a[i + aStart] != b[i])
            return 1;
    }
    return 0;
}

int strlen(const char* a)
{
    int l = 0;
    while(a[l] != '\0')
        l++;
    return l;
}

void strcpy(char* dest, const char* src)
{
    int i = 0;
    while(src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}