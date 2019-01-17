#pragma once

#include "types.h"

void WidenString(char16* dest, char* src);

#define CHECK_ERROR(msg) \
    if(err != EFI_SUCCESS) { \
        printf(msg); \
    }

#define CHECK_ERROR_HALT(msg) \
    if(err != EFI_SUCCESS) { \
        printf(msg); \
        while(1); \
    }

#define CHECK_ERROR_RETURN(msg) \
    if(err != EFI_SUCCESS) { \
        printf(msg); \
        return err; \
    }