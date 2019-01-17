#include "conio.h"

#include "efiutil.h"

static char16* IntToStringBase(int base, char16* buffer, char16* symbols, int64 num) {
    *buffer = '0';

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

static char16* UIntToStringBase(int base, char16* buffer, char16* symbols, uint64 num) {
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

static char16* Int32ToString(int32 num)
{
    static char16 buffer[12] = { 0 };
    static char16 symbols[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };

    return IntToStringBase(10, &buffer[10], symbols, num);
}

static char16* UInt32ToHexString(uint32 num) {
    static char16 buffer[10] = { 0 };
    static char16 symbols[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

    return UIntToStringBase(16, &buffer[8], symbols, num);
}

void printf(const char* format, ...)
{
    static char16 buffer[1024];
    int bufferIndex = 0;

    char* arg = ((char*)&format + 8);

    uint32 i = 0;
    char16 c;
    while((c = format[i]) != '\0') {
        if(c == '%') {
            i++;
            char16 f = format[i];
            i++;

            if(bufferIndex != 0) {
                buffer[bufferIndex] = '\0';
                Print(buffer);
                bufferIndex = 0;
            }

            if(f == 'i') {
                char16* num = Int32ToString(*(int64*)arg);
                Print(num);

                arg += 8;
            } else if(f == 'X') {
                char16* num = UInt32ToHexString(*(uint64*)arg);
                Print(num);

                arg += 8;
            } else if(f == 's') {
                char* str = *(char**)arg;
                arg += 8;

                uint32 strI = 0;
                while(str[strI] != '\0') {
                    buffer[bufferIndex] = str[strI];
                    bufferIndex++;
                    strI++;

                    if(bufferIndex == 1023) {
                        Print(buffer);
                        bufferIndex = 0;
                    }
                }
            }
        } else if(c == '\n') {
            if(bufferIndex > 1021) {
                buffer[bufferIndex] = '\0';
                Print(buffer);
                bufferIndex = 0;
            }

            buffer[bufferIndex++] = '\r';
            buffer[bufferIndex++] = '\n';

            i++;
        } else {
            buffer[bufferIndex] = c;
            bufferIndex++;
            i++;

            if(bufferIndex == 1023) {
                Print(buffer);
                bufferIndex = 0;
            }
        }
    }

    if(bufferIndex != 0) {
        buffer[bufferIndex] = '\0';
        Print(buffer);
    }
}

void Print(char16* msg) 
{
    g_EFISystemTable->ConOut->OutputString(g_EFISystemTable->ConOut, msg);
}
void Println(char16* msg)
{
    Print(msg);
    Print(L"\r\n");
}

void WaitForKey() 
{
    Println(L"Press any key to continue...");

    g_EFISystemTable->ConIn->Reset(g_EFISystemTable->ConIn, FALSE);

    EFI_INPUT_KEY key;
    EFI_STATUS status;
    while((status = g_EFISystemTable->ConIn->ReadKeyStroke(g_EFISystemTable->ConIn, &key)) == EFI_NOT_READY);
}