#include <efi.h>

#include "efiutil.h"
#include "conio.h"
#include "file.h"
#include "KernelHeader.h"

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

    Console::Print(L"Loading modules...\r\n");

    uint64 numModules = 0;
    ModuleDescriptor* modules;
    EFIUtil::SystemTable->BootServices->AllocatePool(EfiLoaderData, 10 * sizeof(ModuleDescriptor), (void**)&modules);

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
                bufferIndex = 0;

                Console::Print(L"Module: ");
                Console::Print((wchar_t*)convBuffer);
                Console::Print(L"\r\n");

                FileIO::FileData moduleData = FileIO::ReadFile((wchar_t*)convBuffer);
                if(moduleData.size == 0) {
                    Console::Print(L"Failed to load module\r\nPress any key to exit...\r\n");
                    EFIUtil::WaitForKey();
                    return EFI_LOAD_ERROR;
                }

                int nameLength = 0;
                while(buffer[nameLength] != '\0')
                    nameLength++;

                if(buffer[nameLength - 4] == '.' && buffer[nameLength - 3] == 's' && buffer[nameLength - 2] == 'y' && buffer[nameLength - 1] == 's') {
                    Console::Print(L"Module is of type ELF\r\n");
                    modules[numModules].type = ModuleDescriptor::TYPE_ELF_IMAGE;
                } else if (buffer[nameLength - 4] == '.' && buffer[nameLength - 3] == 'i' && buffer[nameLength - 2] == 'm' && buffer[nameLength - 1] == 'g') {
                    Console::Print(L"Module is of type RAMDISK\r\n");
                    modules[numModules].type = ModuleDescriptor::TYPE_RAMDISK_IMAGE;
                } else {
                    Console::Print(L"Module is of type UNKNOWN\r\n");
                    modules[numModules].type = ModuleDescriptor::TYPE_UNKNOWN;
                }

                modules[numModules].buffer = moduleData.data;
                modules[numModules].size = moduleData.size;
            }
        } else {
            buffer[bufferIndex] = c;
            bufferIndex++;
        }
    }

    EFIUtil::SystemTable->BootServices->FreePool(configData.data);

    EFIUtil::WaitForKey();
}