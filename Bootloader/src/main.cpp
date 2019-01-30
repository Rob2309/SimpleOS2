#include <efi.h>

#include "efiutil.h"
#include "conio.h"
#include "file.h"

extern "C" EFI_STATUS efi_main(EFI_HANDLE imgHandle, EFI_SYSTEM_TABLE* sysTable)
{
    EFIUtil::SystemTable = sysTable;

    Console::Print(L"Initializing bootloader...\r\n");

    EFI_GUID guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_STATUS err = sysTable->BootServices->HandleProtocol(imgHandle, &guid, (void**)&EFIUtil::LoadedImage);
    if(err != EFI_SUCCESS) {
        Console::Print(L"Failed to init loaded image protocol\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    err = sysTable->BootServices->HandleProtocol(EFIUtil::LoadedImage->DeviceHandle, &guid, (void**)&EFIUtil::FileSystem);
    if(err != EFI_SUCCESS) {
        Console::Print(L"Failed to init filesystem protocol\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    Console::Print(L"Loading config file...\r\n");

    FileIO::FileData configData = FileIO::ReadFile(L"EFI\\BOOT\\config.cfg");
    if(configData.size == 0) {
        Console::Print(L"Failed to load config file\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    char buffer[256] = { 0 };
    char16 convBuffer[256] = { 0 };
    int bufferIndex = 0;
    for(int i = 0; i < configData.size; i++) {
        char c = configData.data[i];
        if(c == '\r' || c == '\n') {
            if(bufferIndex != 0) {
                for(int j = 0; j < bufferIndex; j++)
                    convBuffer[j] = buffer[j];
                convBuffer[bufferIndex] = '\0';
                Console::Print((wchar_t*)convBuffer);
                Console::Print(L"\r\n");
                bufferIndex = 0;
            }
        } else {
            buffer[bufferIndex] = c;
            bufferIndex++;
        }
    }

    EFIUtil::WaitForKey();
}