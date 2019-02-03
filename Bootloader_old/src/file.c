#include "file.h"

#include "util.h"
#include "efiutil.h"

FILE* fopen(char16* path)
{
    EFI_STATUS err; 

    EFI_FILE_HANDLE rootFile;
    err = g_EFIFileSystem->OpenVolume(g_EFIFileSystem, &rootFile);
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

uint64 fgetsize(FILE* file)
{
    EFI_STATUS err;
    EFI_FILE_HANDLE handle = file;

    static EFI_GUID s_FileInfoGUID = EFI_FILE_INFO_ID;
    static char s_Buffer[256];
    static uint64 s_BufferSize = sizeof(s_Buffer);

    uint64 infoSize = s_BufferSize;
    err = handle->GetInfo(handle, &s_FileInfoGUID, &infoSize, s_Buffer);
    
    if(err != EFI_SUCCESS) {
        if(err == EFI_BUFFER_TOO_SMALL)
            printf("fgetsize: buffer too small, needed: %i\n", s_BufferSize);

        return -1;
    }

    return ((EFI_FILE_INFO*)s_Buffer)->FileSize;
}