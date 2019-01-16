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