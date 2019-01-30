#pragma once

#include "types.h"
#include <efi.h>

bool EFIUtilInit(void* imageHandle, void* systemTable);

extern EFI_HANDLE g_EFIImageHandle;
extern EFI_SYSTEM_TABLE* g_EFISystemTable;
extern EFI_LOADED_IMAGE_PROTOCOL* g_EFILoadedImage;
extern EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* g_EFIFileSystem;
extern EFI_GRAPHICS_OUTPUT_PROTOCOL* g_EFIGraphics;