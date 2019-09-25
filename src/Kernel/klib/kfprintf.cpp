#include "stdio.h"

#include "fs/VFS.h"
#include "klib/string.h"

struct Format {
    bool leftJustify;
    bool forceSign;
    bool spaceIfNoSign;
    bool leftPadWithZeroes;
    bool argWidth;
    int minWidth;
    bool argPrecision;
    int precision;
};

static const char* stoui(const char* str, int& outVal) {
    int res = 0;
    int i = 0;
    while(str[i] >= '0' && str[i] <= '9') {
        res *= 10;
        res += str[i] - '0';
        i++;
    }
    outVal = res;
    return &str[i];
}

static void PrintString(uint64 fd, const char* str, int l) {
    VFS::Write(fd, str, l);
}

static void PrintStringMaxLength(uint64 fd, const char* str, int maxLength, int length) {
    if(length > maxLength)
        length = maxLength;

    PrintString(fd, str, length);
}

static void PrintPadding(uint64 fd, int length, bool padWithZeroes) {
    if(padWithZeroes) {
        for(int i = 0; i < length; i++)
            VFS::Write(fd, "0", 1);
    } else {
        for(int i = 0; i < length; i++)
            VFS::Write(fd, " ", 1);
    }
}

static void PrintStringPadded(uint64 fd, const char* str, int length, const Format& fmt) {
    if(length < fmt.minWidth) {
        if(fmt.leftJustify) {
            PrintString(fd, str, length);
            PrintPadding(fd, fmt.minWidth - length, false);
        } else {
            PrintPadding(fd, fmt.minWidth - length, fmt.leftPadWithZeroes);
            PrintString(fd, str, length);
        }
    } else {
        PrintString(fd, str, length);
    }
}

static void PrintStringPaddedPrecision(uint64 fd, const char* str, int length, const Format& fmt) {
    if(length > fmt.precision && fmt.precision != 0) {
        length = fmt.precision;
    }

    if(length < fmt.minWidth) {
        if(fmt.leftJustify) {
            PrintString(fd, str, length);
            PrintPadding(fd, fmt.minWidth - length, false);
        } else {
            PrintPadding(fd, fmt.minWidth - length, fmt.leftPadWithZeroes);
            PrintString(fd, str, length);
        }
    } else {
        PrintString(fd, str, length);
    }
}

static void PrintStringCentered(uint64 fd, const char* str, int length, uint64 padding) {
    if(length > padding) {
        PrintString(fd, str, length);
        return;
    }

    int leftPadding = (padding - length) / 2;
    int rightPadding = padding - length - leftPadding;

    PrintPadding(fd, leftPadding, false);
    PrintString(fd, str, length);
    PrintPadding(fd, rightPadding, false);
}

static const char* ParseFlags(const char* fmt, Format& flags) {
    int i = 0;
    while(true) {
        char c = fmt[i];
        switch(c) {
        case '-': flags.leftJustify = true; break;
        case '+': flags.forceSign = true; break;
        case ' ': flags.spaceIfNoSign = true; break;
        case '0': flags.leftPadWithZeroes = true; break;
        default: return &fmt[i];
        }

        i++;
    }
}

static const char* ParseWidth(const char* fmt, Format& flags) {
    if(fmt[0] == '*') {
        flags.argWidth = true;
        return &fmt[1];
    } else if(fmt[0] >= '0' && fmt[0] <= '9') {
        int width;
        fmt = stoui(fmt, width);
        flags.minWidth = width;
        return fmt;
    } else {
        return fmt;
    }
}

static const char* ParsePrecision(const char* fmt, Format& flags) {
    if(fmt[0] == '.') {
        fmt++;
        if(fmt[0] >= '0' && fmt[0] <= '9') {
            int prec;
            fmt = stoui(fmt, prec);
            flags.precision = prec;
            return fmt;
        } else if(fmt[0] == '*') {
            flags.argPrecision = true;
            return &fmt[1];
        }
    } else {
        return &fmt[0];
    }
}

static void PrintChar(uint64 fd, char c, const Format& fmt) {
    char buf[2] = { c, 0 };
    PrintStringPadded(fd, buf, 1, fmt);
}

static void PrintInt64Base(uint64 fd, int64 val, const char* digits, int base, const Format& fmt) {
    char s_Buffer[128];
    char* buffer = &s_Buffer[127];

    *buffer = '\0';
    buffer--;

    bool neg = false;
    if(val < 0) {
        neg = true;
        val = -val;
    }

    int length = 0;

    do {
        *buffer = digits[(val % base)];
        val /= base;
        buffer--;
        length++;
    } while(val != 0);

    for(int i = length; i < fmt.precision; i++) {
        *buffer = digits[0];
        buffer--;
        length++;
    }

    if(neg) {
        *buffer = '-';
        buffer--;
        length++;
    } else if(fmt.forceSign) {
        *buffer = '+';
        buffer--;
        length++;
    } else if(fmt.spaceIfNoSign) {
        *buffer = ' ';
        buffer--;
        length++;
    }

    PrintStringPadded(fd, buffer + 1, length, fmt);
}

static void PrintUInt64Base(uint64 fd, uint64 val, const char* digits, int base, const Format& fmt) {
    char s_Buffer[128];
    char* buffer = &s_Buffer[127];

    *buffer = '\0';
    buffer--;

    int length = 0;

    do {
        *buffer = digits[(val % base)];
        val /= base;
        buffer--;
        length++;
    } while(val != 0);

    for(int i = length; i < fmt.precision; i++) {
        *buffer = digits[0];
        buffer--;
        length++;
    }

    if(fmt.forceSign) {
        *buffer = '+';
        buffer--;
        length++;
    } else if(fmt.spaceIfNoSign) {
        *buffer = ' ';
        buffer--;
        length++;
    }

    PrintStringPadded(fd, buffer + 1, length, fmt);
}

static void PrintInt(uint64 fd, int64 val, const Format& fmt) {
    static char s_Digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    PrintInt64Base(fd, val, s_Digits, 10, fmt);
}

static void PrintUInt(uint64 fd, int64 val, const Format& fmt) {
    static char s_Digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    PrintUInt64Base(fd, val, s_Digits, 10, fmt);
}

static void PrintOctalInt(uint64 fd, int64 val, const Format& fmt) {
    static char s_Digits[] = { '0', '1', '2', '3', '4', '5', '6', '7' };
    PrintInt64Base(fd, val, s_Digits, 8, fmt);
}

static void PrintHexUIntLower(uint64 fd, int64 val, const Format& fmt) {
    static char s_Digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    PrintUInt64Base(fd, val, s_Digits, 16, fmt);
}

static void PrintHexUIntUpper(uint64 fd, int64 val, const Format& fmt) {
    static char s_Digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    PrintUInt64Base(fd, val, s_Digits, 16, fmt);
}

static void _kfvprintf(uint64 fd, const char* format, __builtin_va_list arg) {
    uint32 i = 0;
    char c;
    while((c = format[i]) != '\0') {
        if(c == '%') {
            Format outFormat = { 0 };

            i++;
            const char* tmp = ParseFlags(&format[i], outFormat);
            tmp = ParseWidth(tmp, outFormat);
            tmp = ParsePrecision(tmp, outFormat);

            if(outFormat.argWidth)
                outFormat.minWidth = __builtin_va_arg(arg, uint64);
            if(outFormat.argPrecision)
                outFormat.precision = __builtin_va_arg(arg, uint64);

            char f = tmp[0];
            tmp++;
            if(f == 'S') {
                char* str = __builtin_va_arg(arg, char*);
                int l = kstrlen(str);
                uint64 padding = __builtin_va_arg(arg, uint64);
                PrintStringCentered(fd, str, l, padding);
            } else if(f == 'c') {
                char val = __builtin_va_arg(arg, uint64);
                PrintChar(fd, val, outFormat);
            } else if(f == 'd' || f == 'i') {
                int64 val = __builtin_va_arg(arg, int64);
                PrintInt(fd, val, outFormat);
            } else if(f == 'o') {
                int64 val = __builtin_va_arg(arg, int64);
                PrintOctalInt(fd, val, outFormat);
            } else if(f == 's') {
                const char* val = __builtin_va_arg(arg, const char*);
                PrintStringPaddedPrecision(fd, val, kstrlen(val), outFormat);
            } else if(f == 'u') {
                uint64 val = __builtin_va_arg(arg, uint64);
                PrintUInt(fd, val, outFormat);
            } else if(f == 'x') {
                uint64 val = __builtin_va_arg(arg, uint64);
                PrintHexUIntLower(fd, val, outFormat);
            } else if(f == 'X' || f == 'p') {
                uint64 val = __builtin_va_arg(arg, uint64);
                PrintHexUIntUpper(fd, val, outFormat);
            } else if(f == 'n') {

            } else if(f == '%') {
                VFS::Write(fd, "%", 1);
            }

            i += (uint64)tmp - (uint64)&format[i];
        } else {
            VFS::Write(fd, &c, 1);
            i++;
        }
    }
}

void kfprintf(uint64 fd, const char* format, ...) {
    __builtin_va_list arg;
    __builtin_va_start(arg, format);

    kfvprintf(fd, format, arg);

    __builtin_va_end(arg);
}
void kfvprintf(uint64 fd, const char* format, __builtin_va_list arg) {
    _kfvprintf(fd, format, arg);
}