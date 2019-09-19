#include "string.h"

void* memcpy(void* dest, const void* src, uint64 num) {
    if(num == 0)
        return dest;
    for(uint64 i = 0; i < num; i++)
        ((char*)dest)[i] = ((char*)src)[i];
    return dest;
}
void* memmove(void* dest, const char* src, uint64 num) {
    if(dest == src)
        return dest;
    if(num == 0)
        return dest;

    if(dest > src) {
        for(uint64 i = num - 1; i >= 0; i--)
            ((char*)dest)[i] = ((char*)src)[i];
    } else {
        return memcpy(dest, src, num);
    }
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