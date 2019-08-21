#include "stdio.h"

#include "terminal/terminal.h"
#include "locks/StickyLock.h"

#include "klib/string.h"

#include "syscalls/SyscallDefine.h"
#include "memory/MemoryManager.h"
#include "scheduler/Scheduler.h"

static uint32 g_TerminalColor = 0xFFFFFF;
static StickyLock g_PrintLock;

extern Terminal::TerminalInfo g_TerminalInfo;

struct Format {
    bool leftJustify;
    bool forceSign;
    bool spaceIfNoSign;
    bool addPrefix;
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

static void PrintString(const char* str, uint32 color) {
    int i = 0;
    while(str[i] != '\0') {
        if(str[i] == '\n')
            Terminal::NewLine(&g_TerminalInfo);
        else
            Terminal::PutChar(&g_TerminalInfo, str[i], color);
        i++;    
    }
}

static void PrintPadding(int length, bool padWithZeroes, uint32 color) {
    if(padWithZeroes) {
        for(int i = 0; i < length; i++)
            Terminal::PutChar(&g_TerminalInfo, '0', color);
    } else {
        for(int i = 0; i < length; i++)
            Terminal::PutChar(&g_TerminalInfo, ' ', color);
    }
}

static void PrintStringPadded(const char* str, int length, const Format& fmt, uint32 color) {
    if(length < fmt.minWidth) {
        if(fmt.leftJustify) {
            PrintString(str, color);
            PrintPadding(fmt.minWidth - length, false, color);
        } else {
            PrintPadding(fmt.minWidth - length, fmt.leftPadWithZeroes, color);
            PrintString(str, color);
        }
    } else {
        PrintString(str, color);
    }
}

static void PrintStringCentered(const char* str, uint32 color, uint64 padding) {
    int len = kstrlen(str);
    if(len > padding) {
        PrintString(str, color);
        return;
    }

    int leftPadding = (padding - len) / 2;
    int rightPadding = padding - len - leftPadding;

    for(int i = 0; i < leftPadding; i++)
        Terminal::PutChar(&g_TerminalInfo, ' ', color);
    for(int i = 0; i < len; i++)
        Terminal::PutChar(&g_TerminalInfo, str[i], color);
    for(int i = 0; i < rightPadding; i++)
        Terminal::PutChar(&g_TerminalInfo, ' ', color);
}

static const char* ParseFlags(const char* fmt, Format& flags) {
    int i = 0;
    while(true) {
        char c = fmt[i];
        switch(c) {
        case '-': flags.leftJustify = true; break;
        case '+': flags.forceSign = true; break;
        case ' ': flags.spaceIfNoSign = true; break;
        case '#': flags.addPrefix = true; break;
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

static void PrintChar(char c, const Format& fmt, uint32 color) {
    char buf[2] = { c, 0 };
    PrintStringPadded(buf, 1, fmt, color);
}

static void PrintInt64Base(int64 val, const char* digits, int base, const Format& fmt, uint32 color) {
    static char s_Buffer[128];
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

    if(fmt.addPrefix) {
        if(base == 16) {
            *buffer = 'x';
            buffer--;
            *buffer = '0';
            buffer--;
            length += 2;
        } else if(base == 8) {
            *buffer = '0';
            buffer--;
            length++;
        }
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

    PrintStringPadded(buffer + 1, length, fmt, color);
}

static void PrintUInt64Base(uint64 val, const char* digits, int base, const Format& fmt, uint32 color) {
    static char s_Buffer[128];
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

    if(fmt.addPrefix) {
        if(base == 16) {
            *buffer = 'x';
            buffer--;
            *buffer = '0';
            buffer--;
            length += 2;
        } else if(base == 8) {
            *buffer = '0';
            buffer--;
            length++;
        }
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

    PrintStringPadded(buffer + 1, length, fmt, color);
}

static void PrintInt(int64 val, const Format& fmt, uint32 color) {
    static char s_Digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    PrintInt64Base(val, s_Digits, 10, fmt, color);
}

static void PrintUInt(int64 val, const Format& fmt, uint32 color) {
    static char s_Digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    PrintUInt64Base(val, s_Digits, 10, fmt, color);
}

static void PrintOctalInt(int64 val, const Format& fmt, uint32 color) {
    static char s_Digits[] = { '0', '1', '2', '3', '4', '5', '6', '7' };
    PrintInt64Base(val, s_Digits, 8, fmt, color);
}

static void PrintHexUIntLower(int64 val, const Format& fmt, uint32 color) {
    static char s_Digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    PrintUInt64Base(val, s_Digits, 16, fmt, color);
}

static void PrintHexUIntUpper(int64 val, const Format& fmt, uint32 color) {
    static char s_Digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    PrintUInt64Base(val, s_Digits, 16, fmt, color);
}

void kprintf(const char* format, ...)
{
    __builtin_va_list arg;
    __builtin_va_start(arg, format);

    g_PrintLock.Spinlock_Raw();

    uint32 color = g_TerminalColor;

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
                uint64 padding = __builtin_va_arg(arg, uint64);
                PrintStringCentered(str, color, padding);
            } else if(f == 'C') {
                uint8 r = __builtin_va_arg(arg, uint64);
                uint8 g = __builtin_va_arg(arg, uint64);
                uint8 b = __builtin_va_arg(arg, uint64);
                color = (r << 16) | (g << 8) | b;
            } else if(f == 'c') {
                char val = __builtin_va_arg(arg, uint64);
                PrintChar(val, outFormat, color);
            } else if(f == 'd' || f == 'i') {
                int64 val = __builtin_va_arg(arg, int64);
                PrintInt(val, outFormat, color);
            } else if(f == 'o') {
                int64 val = __builtin_va_arg(arg, int64);
                PrintOctalInt(val, outFormat, color);
            } else if(f == 's') {
                const char* val = __builtin_va_arg(arg, const char*);
                PrintStringPadded(val, kstrlen(val), outFormat, color);
            } else if(f == 'u') {
                uint64 val = __builtin_va_arg(arg, uint64);
                PrintUInt(val, outFormat, color);
            } else if(f == 'x') {
                uint64 val = __builtin_va_arg(arg, uint64);
                PrintHexUIntLower(val, outFormat, color);
            } else if(f == 'X' || f == 'p') {
                uint64 val = __builtin_va_arg(arg, uint64);
                PrintHexUIntUpper(val, outFormat, color);
            } else if(f == 'n') {

            } else if(f == '%') {
                Terminal::PutChar(&g_TerminalInfo, '%', color);
            }

            i += (uint64)tmp - (uint64)&format[i];
        } else if(c == '\n') {
            Terminal::NewLine(&g_TerminalInfo);
            i++;
        } else {
            Terminal::PutChar(&g_TerminalInfo, c, color);
            i++;
        }
    }

    g_PrintLock.Unlock_Raw();

    __builtin_va_end(arg);
}

void kprintf_setcolor(uint32 color)
{
    g_TerminalColor = color;
}

void kprintf_setcolor(uint8 r, uint8 g, uint8 b)
{
    kprintf_setcolor((r << 16) | (g << 8) | b);
}

SYSCALL_DEFINE1(syscall_print, const char* msg) {
    if(!MemoryManager::IsUserPtr(msg))
        Scheduler::ThreadKillProcess("InvalidUserPointer");
    kprintf(msg);
    return 0;
}