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
            i++;
            char f = format[i];
            i++;

            if(f == 'i') {
                char* num = Int64ToString(__builtin_va_arg(arg, int64));
                PrintString(num, color);
            } else if(f == 'X') {
                char* num = UInt64ToHexString(__builtin_va_arg(arg, uint64));
                PrintString(num, color);
            } else if(f == 'x') {
                char* num = UInt64ToHexStringPadded(__builtin_va_arg(arg, uint64));
                PrintString(num, color);
            } else if(f == 's') {
                char* str = __builtin_va_arg(arg, char*);
                PrintString(str, color);
            } else if(f == 'S') {
                char* str = __builtin_va_arg(arg, char*);
                uint64 padding = __builtin_va_arg(arg, uint64);
                PrintStringCentered(str, color, padding);
            } else if(f == 'c') {
                uint32 col = __builtin_va_arg(arg, uint64);
                color = col;
            } else if(f == 'C') {
                uint8 r = __builtin_va_arg(arg, uint64);
                uint8 g = __builtin_va_arg(arg, uint64);
                uint8 b = __builtin_va_arg(arg, uint64);
                color = (r << 16) | (g << 8) | b;
            }
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

SYSCALL_DEFINE(syscall_print) {
    const char* msg = (const char*)arg1;
    if(!MemoryManager::IsUserPtr(msg))
        Scheduler::ThreadKillProcess("InvalidUserPointer");
    kprintf(msg);
    return 0;
}