#include "string.h"

int64 strlen(const char* str) {
    int64 len = 0;
    while(str[len] != 0)
        len++;
    return len;
}