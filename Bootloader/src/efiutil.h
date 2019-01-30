#pragma once

#include <efi.h>

namespace EFIUtil {

    extern EFI_SYSTEM_TABLE* SystemTable;
    extern EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
    extern EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;

    void WaitForKey();

}