#include "conio.h"

#include "terminal.h"
#include "types.h"

static char* IntToStringBase(int base, char* buffer, char* symbols, int64 num) {
    if(num == 0) {
        *buffer = '0';
        return buffer;
    }

    bool neg = num < 0;
    if(neg)
        num = -num;

    while(num != 0) {
        int r = num % base;
        num /= base;
        *buffer = symbols[r];
        buffer--;
    }

    if(neg) {
        *buffer = '-';
        buffer--;
    }

    return buffer + 1;
}

static char* UIntToStringBase(int base, char* buffer, char* symbols, uint64 num) {
    if(num == 0) {
        *buffer = '0';
        return buffer;
    }

    while(num != 0) {
        int r = num % base;
        num /= base;
        *buffer = symbols[r];
        buffer--;
    }

    return buffer + 1;
}

static char* Int64ToString(int64 num)
{
    static char buffer[40] = { 0 };
    static char symbols[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };

    return IntToStringBase(10, &buffer[38], symbols, num);
}

static char* UInt64ToHexString(uint64 num) {
    static char buffer[20] = { 0 };
    static char symbols[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

    return UIntToStringBase(16, &buffer[18], symbols, num);
}

static char* UInt64ToHexStringPadded(uint64 num) {
    static char buffer[17] = { 0 };
    static char symbols[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

    char* ptr = UIntToStringBase(16, &buffer[15], symbols, num);

    for(char* i = &buffer[0]; i != ptr; i++)
        *i = '0';
    
    return &buffer[0];
}

static void PrintString(const char* str) {
    int i = 0;
    while(str[i] != '\0') {
        Terminal::PutChar(str[i]);
        i++;    
    }
}

void printf(const char* format, ...)
{
    __builtin_va_list arg;
    __builtin_va_start(arg, format);

    uint32 i = 0;
    char c;
    while((c = format[i]) != '\0') {
        if(c == '%') {
            i++;
            char f = format[i];
            i++;

            if(f == 'i') {
                char* num = Int64ToString(__builtin_va_arg(arg, int64));
                PrintString(num);
            } else if(f == 'X') {
                char* num = UInt64ToHexString(__builtin_va_arg(arg, uint64));
                PrintString(num);
            } else if(f == 'x') {
                char* num = UInt64ToHexStringPadded(__builtin_va_arg(arg, uint64));
                PrintString(num);
            } else if(f == 's') {
                char* str = __builtin_va_arg(arg, char*);
                PrintString(str);
            }
        } else if(c == '\n') {
            Terminal::NewLine();
            i++;
        } else {
            Terminal::PutChar(c);
            i++;
        }
    }

    __builtin_va_end(arg);
}