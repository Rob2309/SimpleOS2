#include "conio.h"

#include "console.h"
#include "efiutil.h"

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

static char16* UIntToStringBase(int base, char* buffer, char* symbols, uint64 num) {
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

static char* Int32ToString(int32 num)
{
    static char buffer[12] = { 0 };
    static char symbols[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };

    return IntToStringBase(10, &buffer[10], symbols, num);
}

static char* UInt32ToHexString(uint32 num) {
    static char buffer[10] = { 0 };
    static char symbols[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

    return UIntToStringBase(16, &buffer[8], symbols, num);
}

void printf(const char* format, ...)
{
    char* arg = ((char*)&format + 8);

    uint32 i = 0;
    char c;
    while((c = format[i]) != '\0') {
        if(c == '%') {
            i++;
            char f = format[i];
            i++;

            if(f == 'i') {
                char* num = Int32ToString(*(int64*)arg);
                Print(num);

                arg += 8;
            } else if(f == 'X') {
                char* num = UInt32ToHexString(*(uint64*)arg);
                Print(num);

                arg += 8;
            } else if(f == 's') {
                char* str = *(char**)arg;
                arg += 8;

                Print(str);
            }
        } else if(c == '\n') {
            NewLine();

            i++;
        } else {
            PutChar(c);

            i++;
        }
    }
}

void Print(char* msg) 
{
    int i = 0;
    while(msg[i] != '\0') {
        if(msg[i] == '\n')
            NewLine();
        else
            PutChar(msg[i]);    
    }
}
void Println(char* msg)
{
    Print(msg);
    Print("\n");
}

void WaitForKey() 
{
    g_EFISystemTable->ConIn->Reset(g_EFISystemTable->ConIn, FALSE);

    EFI_INPUT_KEY key;
    EFI_STATUS status;
    while((status = g_EFISystemTable->ConIn->ReadKeyStroke(g_EFISystemTable->ConIn, &key)) == EFI_NOT_READY);
}