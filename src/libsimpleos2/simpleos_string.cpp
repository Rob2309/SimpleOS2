#include "simpleos_string.h"

void* memcpy(void* dest, const void* src, int64 num) {
    if(num == 0)
        return dest;
    for(int64 i = 0; i < num; i++)
        ((char*)dest)[i] = ((char*)src)[i];
    return dest;
}
void* memmove(void* vDest, const void* vSrc, int64 num) {
    char* dest = (char*)vDest;
    const char* src = (const char*)vSrc;

    if(dest == src || num == 0)
        return dest;

    if(dest > src && (src + num) > dest) {
        for(int64 i = num - 1; i >= 0; i--)
            ((char*)dest)[i] = ((char*)src)[i];
    } else {
        memcpy(dest, src, num);
    }

    return dest;
}

char* strcpy(char* dest, const char* src) {
    int index = 0;
    while(true) {
        char c = src[index];
        dest[index] = c;
        if(c == '\0')
            break;
        index++; 
    }
    return dest;
}

char* strcat(char* dest, const char* src) {
    int start = strlen(dest);
    int len = strlen(src) + 1;

    for(int i = 0; i < len; i++) {
        dest[i + start] = src[i];
    }
    return dest;
}

int64 strlen(const char* str) {
    int64 len = 0;
    while(str[len] != 0)
        len++;
    return len;
}

int64 strcmp(const char* a, const char* b) {
    while(*a == *b) {
        if(*a == '\0')
            return 0;

        a++;
        b++;
    }

    return *a - *b;
}