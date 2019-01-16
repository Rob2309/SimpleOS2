#pragma once

#include <efi.h>

void WidenString(CHAR16* dest, char* src);

void Println(CHAR16* msg);
void WaitForKey();

#define CHECK_ERROR(msg) \
    if(err != EFI_SUCCESS) { \
        Println(msg); \
    }

#define CHECK_ERROR_RETURN(msg) \
    if(err != EFI_SUCCESS) { \
        Println(msg); \
        return err; \
    }