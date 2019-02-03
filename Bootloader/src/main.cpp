#include <efi.h>

#include "efiutil.h"
#include "conio.h"
#include "file.h"
#include "KernelHeader.h"
#include "ELF.h"
#include "allocator.h"

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

    guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    err = sysTable->BootServices->LocateProtocol(&guid, nullptr, (void**)&EFIUtil::Graphics);
    if(err != EFI_SUCCESS) {
        Console::Print(L"Failed to init grpahics protocol\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    Console::Print(L"Switching video mode...\r\n");

    UINT32 resBestX = 0;
    UINT32 resBestY = 0;
    UINT32 resBestScanlineWidth = 0;
    UINT32 resBestMode = 0;
    for(UINT32 m = 0; m < EFIUtil::Graphics->Mode->MaxMode; m++) {
        UINTN infoSize;
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info;
        EFIUtil::Graphics->QueryMode(EFIUtil::Graphics, m, &infoSize, &info);

        if(info->VerticalResolution > resBestY && info->HorizontalResolution <= 1920 && info->VerticalResolution <= 1080 && (info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor || info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor)) {
            resBestX = info->HorizontalResolution;
            resBestY = info->VerticalResolution;
            resBestScanlineWidth = info->PixelsPerScanLine;
            resBestMode = m;
        }
    }

    EFIUtil::Graphics->SetMode(EFIUtil::Graphics, resBestMode);

    Console::Print(L"Loading config file...\r\n");

    FileIO::FileData configData = FileIO::ReadFile(L"EFI\\BOOT\\config.cfg");
    if(configData.size == 0) {
        Console::Print(L"Failed to load config file\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    Console::Print(L"Loading modules...\r\n");

    uint64 numModules = 0;
    ModuleDescriptor* modules = (ModuleDescriptor*)Allocate(10 * sizeof(ModuleDescriptor));

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

                if (buffer[nameLength - 4] == '.' && buffer[nameLength - 3] == 'i' && buffer[nameLength - 2] == 'm' && buffer[nameLength - 1] == 'g') {
                    Console::Print(L"Module is of type RAMDISK\r\n");
                    modules[numModules].type = ModuleDescriptor::TYPE_RAMDISK_IMAGE;
                } else if(buffer[nameLength - 4] == '.' && buffer[nameLength - 3] == 's' && buffer[nameLength - 2] == 'y' && buffer[nameLength - 1] == 's') {
                    Console::Print(L"Module is of type ELF\r\n");
                    modules[numModules].type = ModuleDescriptor::TYPE_ELF_IMAGE;
                } else {
                    Console::Print(L"Module is of type UNKNOWN\r\n");
                    modules[numModules].type = ModuleDescriptor::TYPE_UNKNOWN;
                }

                modules[numModules].buffer = moduleData.data;
                modules[numModules].size = moduleData.size;
                numModules++;
            }
        } else {
            buffer[bufferIndex] = c;
            bufferIndex++;
        }
    }

    Free(configData.data, configData.size);

    Elf64Addr kernelEntryPoint = 0;

    Console::Print(L"Preparing modules...\r\n");

    for(int i = 0; i < numModules; i++) {
        ModuleDescriptor* desc = &modules[i];

        if(desc->type == ModuleDescriptor::TYPE_ELF_IMAGE) {
            Console::Print(L"Preparing ELF module...\r\n");

            uint64 size = GetELFSize(desc->buffer);
            uint8* processBuffer = (uint8*)Allocate(size);
            
            if(!PrepareELF(desc->buffer, processBuffer, &kernelEntryPoint)) {
                Console::Print(L"Failed to prepare ELF module\r\nPress any key to exit...\r\n");
                EFIUtil::WaitForKey();
                return EFI_LOAD_ERROR;
            }

            Free(desc->buffer, desc->size);

            desc->buffer = processBuffer;
            desc->size = size;
        }
    }

    if(kernelEntryPoint == 0) {
        Console::Print(L"No Kernel entry point found\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }


    KernelHeader* header = (KernelHeader*)Allocate(sizeof(KernelHeader));

    header->numModules = numModules;
    header->modules = modules;

    header->screenWidth = resBestX;
    header->screenHeight = resBestY;
    header->screenScanlineWidth = resBestScanlineWidth;
    header->screenBuffer = (uint32*)EFIUtil::Graphics->Mode->FrameBufferBase;

    Console::Print(L"Exiting Boot services and starting kernel...\r\nPress any key to continue...\r\n");
    EFIUtil::WaitForKey();

    UINTN memoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR* memMap;
    UINTN mapKey;
    UINTN descSize;
    UINT32 descVersion;
    err = EFIUtil::SystemTable->BootServices->GetMemoryMap(&memoryMapSize, memMap, &mapKey, &descSize, &descVersion);
    if(err != EFI_BUFFER_TOO_SMALL) {
        Console::Print(L"Failed to get memory map size\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }
    memMap = (EFI_MEMORY_DESCRIPTOR*)Allocate(memoryMapSize + 4096);
    memoryMapSize += 4096;
    err = EFIUtil::SystemTable->BootServices->GetMemoryMap(&memoryMapSize, memMap, &mapKey, &descSize, &descVersion);
    if(err != EFI_SUCCESS) {
        Console::Print(L"Failed to query memory map\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    header->memMap = (MemoryDescriptor*)memMap;
    header->memMapLength = memoryMapSize / descSize;
    header->memMapDescriptorSize = descSize;

    typedef void (*KernelMain)(KernelHeader* header);
    KernelMain kernelMain = (KernelMain)kernelEntryPoint;

    err = EFIUtil::SystemTable->BootServices->ExitBootServices(EFIUtil::LoadedImage, mapKey);
    if(err != EFI_SUCCESS) {
        Console::Print(L"Failed to exit boot services\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    kernelMain(header);
}