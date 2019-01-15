#include <efi.h>
#include <efilib.h>

EFI_GUID g_ProtocolLoadedModule = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID g_ProtocolSimpleFileSystem = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

#define CHECK_ERROR(msg) \
    if(err != EFI_SUCCESS) { \
        sysTable->ConOut->OutputString(sysTable->ConOut, msg); \
        return err; \
    }

void WidenString(CHAR16* dest, char* src)
{
    int i = 0;
    while((dest[i] = src[i]) != '\0')
        i++;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE imgHandle, EFI_SYSTEM_TABLE* sysTable)
{
    
    EFI_STATUS err;

    EFI_LOADED_IMAGE_PROTOCOL* protoLoadedImage;
    err = sysTable->BootServices->HandleProtocol(imgHandle, &g_ProtocolLoadedModule, &protoLoadedImage);
    CHECK_ERROR(L"Failed to get LoadedImage Protocol\r\n");

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* protoFileSystem;
    err = sysTable->BootServices->HandleProtocol(protoLoadedImage->DeviceHandle, &g_ProtocolSimpleFileSystem, &protoFileSystem);
    CHECK_ERROR(L"Failed to get FileSystem Protocol\r\n");

    EFI_FILE_HANDLE rootFile;
    err = protoFileSystem->OpenVolume(protoFileSystem, &rootFile);
    CHECK_ERROR(L"Failed to open boot volume\r\n");

    EFI_FILE_HANDLE txtFile;
    err = rootFile->Open(rootFile, &txtFile, L"msg.txt", EFI_FILE_MODE_READ, 0);
    CHECK_ERROR(L"Failed to open file\r\n");

    UINTN bufSize = 256;
    char buffer[bufSize + 1];
    err = txtFile->Read(txtFile, &bufSize, buffer);
    CHECK_ERROR(L"Failed to read file\r\n");

    CHAR16 convBuffer[bufSize + 1];
    WidenString(convBuffer, buffer);

    sysTable->ConOut->OutputString(sysTable->ConOut, convBuffer);

    return EFI_SUCCESS;
}