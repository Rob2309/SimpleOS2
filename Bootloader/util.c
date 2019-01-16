#include "util.h"

#include "efiutil.h"

void WidenString(CHAR16* dest, char* src)
{
    int i = 0;
    while((dest[i] = src[i]) != '\0')
        i++;
}

void Println(CHAR16* msg)
{
    g_EFISystemTable->ConOut->OutputString(g_EFISystemTable->ConOut, msg);
    g_EFISystemTable->ConOut->OutputString(g_EFISystemTable->ConOut, L"\r\n");
}

void WaitForKey() 
{
    Println(L"Press any key to continue...");

    g_EFISystemTable->ConIn->Reset(g_EFISystemTable->ConIn, FALSE);

    EFI_INPUT_KEY key;
    EFI_STATUS status;
    while((status = g_EFISystemTable->ConIn->ReadKeyStroke(g_EFISystemTable->ConIn, &key)) == EFI_NOT_READY);
}
