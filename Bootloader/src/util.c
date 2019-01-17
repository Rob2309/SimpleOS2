#include "util.h"

void WidenString(char16* dest, char* src)
{
    int i = 0;
    while((dest[i] = src[i]) != '\0')
        i++;
}
