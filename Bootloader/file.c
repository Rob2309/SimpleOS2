#include "file.h"

#include <efi.h>

#include "util.h"

extern EFI_SYSTEM_TABLE* g_SystemTable;
extern EFI_HANDLE g_ImageHandle;

FILE* fopen(char16* path)
{
    EFI_STATUS err; 
    
    EFI_LOADED_IMAGE_PROTOCOL* loadedImage;
    EFI_GUID loadedImageGUID = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    err = g_SystemTable->BootServices->HandleProtocol(g_ImageHandle, &loadedImageGUID, &loadedImage);
    CHECK_ERROR(L"Failed to retrieve LoadedImage Protocol");

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fileSystem;
    EFI_GUID fsGUID = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    err = g_SystemTable->BootServices->HandleProtocol(loadedImage->DeviceHandle, &fsGUID, &fileSystem);
    CHECK_ERROR(L"Failed to retrieve FileSystem Protocol");

    EFI_FILE_HANDLE rootFile;
    err = fileSystem->OpenVolume(fileSystem, &rootFile);
    CHECK_ERROR(L"Failed to open volume root");

    EFI_FILE_HANDLE file;
    err = rootFile->Open(rootFile, &file, path, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    CHECK_ERROR(L"Failed to open file");

    if(err != EFI_SUCCESS)
        return 0;
    else
        return file;
}
void fclose(FILE* file)
{
    ((EFI_FILE_HANDLE)file)->Close(file);
}

int fread(uint8* buffer, uint64 bufferSize, FILE* file)
{
    EFI_STATUS err;
    EFI_FILE_HANDLE handle = file;

    err = handle->Read(handle, &bufferSize, buffer);
    CHECK_ERROR(L"Failed to read from file");

    return bufferSize;
}