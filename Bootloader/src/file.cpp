#include "file.h"

#include "efiutil.h"

namespace FileIO {

    FileData ReadFile(const wchar_t* path)
    {
        EFI_FILE_HANDLE volumeRoot;
        EFIUtil::FileSystem->OpenVolume(EFIUtil::FileSystem, &volumeRoot);

        EFI_FILE_HANDLE file;
        EFI_STATUS err = volumeRoot->Open(volumeRoot, &file, (CHAR16*)path, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
        if(err != EFI_SUCCESS) {
            return { 0, nullptr };
        }

        UINTN infoSize = 0;
        EFI_FILE_INFO* info;

        EFI_GUID guid = EFI_FILE_INFO_ID;
        err = file->GetInfo(file, &guid, &infoSize, nullptr);
        if(err != EFI_BUFFER_TOO_SMALL) {
            return { 0, nullptr };
        }
        EFIUtil::SystemTable->BootServices->AllocatePool(EfiLoaderData, infoSize, (void**)&info);
        err = file->GetInfo(file, &guid, &infoSize, info);
        if(err != EFI_SUCCESS) {
            return { 0, nullptr };
        }

        uint64 size = info->FileSize;
        EFIUtil::SystemTable->BootServices->FreePool(info);

        uint8* buffer;
        EFIUtil::SystemTable->BootServices->AllocatePool(EfiLoaderData, size, (void**)&buffer);

        err = file->Read(file, &size, buffer);
        if(err != EFI_SUCCESS) {
            return { 0, nullptr };
        }

        return { size, buffer };
    }

}