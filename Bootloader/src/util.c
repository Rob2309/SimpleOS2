#include "util.h"

#include "efiutil.h"

void WidenString(CHAR16* dest, char* src)
{
    int i = 0;
    while((dest[i] = src[i]) != '\0')
        i++;
}

void Print(CHAR16* msg) 
{
    g_EFISystemTable->ConOut->OutputString(g_EFISystemTable->ConOut, msg);
}
void Println(CHAR16* msg)
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

CHAR16* ToString(UINT64 num)
{
    static CHAR16 buffer[128] = { 0 };

    int digit = 126;
    buffer[digit] = '0';

    while(num > 0) {
        int rest = num % 10;
        buffer[digit] = '0' + rest;
        digit--;
        
        num /= 10;
    }

    return &buffer[digit + 1];
}

char* malloc(uint64 size)
{
    size += sizeof(uint64);

    uint64 numPages = (size + 4095) / 4096 * 4096;
    uint64 addr = 1;

    EFI_STATUS err;
    err = g_EFISystemTable->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, numPages, &addr);
    if(err != EFI_SUCCESS)
        return 0;

    *(uint64*)addr = numPages;
    return addr + sizeof(uint64);
}

void free(char* block)
{
    block -= sizeof(uint64);
    uint64 numPages = *(uint64*)block;

    g_EFISystemTable->BootServices->FreePages(block, numPages);
}