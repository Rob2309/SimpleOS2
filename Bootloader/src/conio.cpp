#include "conio.h"

#include "efiutil.h"

namespace Console {

    void Print(const wchar_t* msg)
    {
        ::EFIUtil::SystemTable->ConOut->OutputString(::EFIUtil::SystemTable->ConOut, (CHAR16*)msg);
    }

}