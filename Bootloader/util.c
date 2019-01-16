#include "util.h"

extern EFI_SYSTEM_TABLE* g_SystemTable;
extern EFI_HANDLE g_ImageHandle;

void WidenString(CHAR16* dest, char* src)
{
    int i = 0;
    while((dest[i] = src[i]) != '\0')
        i++;
}

void Println(CHAR16* msg)
{
    g_SystemTable->ConOut->OutputString(g_SystemTable->ConOut, msg);
    g_SystemTable->ConOut->OutputString(g_SystemTable->ConOut, L"\r\n");
}

void WaitForKey() 
{
    Println(L"Press any key to continue...");

    g_SystemTable->ConIn->Reset(g_SystemTable->ConIn, FALSE);

    EFI_INPUT_KEY key;
    EFI_STATUS status;
    while((status = g_SystemTable->ConIn->ReadKeyStroke(g_SystemTable->ConIn, &key)) == EFI_NOT_READY);
}
