#pragma once

#include <efi.h>

void WidenString(CHAR16* dest, char* src);

void Print(CHAR16* msg);
void Println(CHAR16* msg);
void WaitForKey();

CHAR16* ToString(UINT64 num);

#define CHECK_ERROR(msg) \
    if(err != EFI_SUCCESS) { \
        Println(msg); \
    }

#define CHECK_ERROR_HALT(msg) \
    if(err != EFI_SUCCESS) { \
        Println(msg); \
        while(1); \
    }

#define CHECK_ERROR_RETURN(msg) \
    if(err != EFI_SUCCESS) { \
        Println(msg); \
        return err; \
    }