#include "efiutil.h"

#include <efi.h>

EFI_HANDLE g_EFIImageHandle;
EFI_SYSTEM_TABLE* g_EFISystemTable;

EFI_LOADED_IMAGE_PROTOCOL* g_EFILoadedImage;
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* g_EFIFileSystem;
EFI_GRAPHICS_OUTPUT_PROTOCOL* g_EFIGraphics;

bool EFIUtilInit(void* imageHandle, void* systemTable)
{
    g_EFIImageHandle = imageHandle;
    g_EFISystemTable = systemTable;

    EFI_GUID loadedImageGUID = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    g_EFISystemTable->BootServices->HandleProtocol(g_EFIImageHandle, &loadedImageGUID, &g_EFILoadedImage);

    EFI_GUID fileSystemGUID = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    g_EFISystemTable->BootServices->HandleProtocol(g_EFILoadedImage->DeviceHandle, &fileSystemGUID, &g_EFIFileSystem);

    EFI_GUID gfxGUID = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    g_EFISystemTable->BootServices->LocateProtocol(&gfxGUID, NULL, &g_EFIGraphics);

    return 1;
}