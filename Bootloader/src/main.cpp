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

    Console::Print(L"Loading modules...\r\n");

    KernelHeader* header = (KernelHeader*)Allocate(sizeof(KernelHeader), MemoryType::KernelHeader);

    FileIO::FileData kernelData = FileIO::ReadFile(L"EFI\\BOOT\\kernel.sys", MemoryType::LoaderData);
    if(kernelData.size == 0) {
        Console::Print(L"Failed to load kernel image\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }
    
    FileIO::FileData fontData = FileIO::ReadFile(L"EFI\\BOOT\\font.img", MemoryType::Font);
    if(fontData.size == 0) {
        Console::Print(L"Failed to load font\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    FileIO::FileData testData = FileIO::ReadFile(L"EFI\\BOOT\\Test.elf", MemoryType::TestImage);
    if(testData.size == 0) {
        Console::Print(L"Failed to load test program\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    Console::Print(L"Preparing kernel...\r\n");

    Elf64Addr kernelEntryPoint = 0;
    uint64 size = GetELFSize(kernelData.data);
    uint8* processBuffer = (uint8*)Allocate(size, MemoryType::KernelImage);
    
    if(!PrepareELF(kernelData.data, processBuffer, &kernelEntryPoint)) {
        Console::Print(L"Failed to prepare kernel\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    Free(kernelData.data, kernelData.size);

    header->kernelImage.buffer = processBuffer;
    header->kernelImage.size = size;

    header->fontImage.buffer = fontData.data;
    header->fontImage.size = fontData.size;

    header->helloWorldImage.buffer = testData.data;
    header->helloWorldImage.size = testData.size;

    if(kernelEntryPoint == 0) {
        Console::Print(L"Kernel entry point not found\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    header->screenWidth = resBestX;
    header->screenHeight = resBestY;
    header->screenScanlineWidth = resBestScanlineWidth;
    header->screenBuffer = (uint32*)EFIUtil::Graphics->Mode->FrameBufferBase;
    header->screenBufferSize = EFIUtil::Graphics->Mode->FrameBufferSize;

    void* newStack = Allocate(16 * 4096, MemoryType::KernelStack);
    header->stack = newStack;
    header->stackSize = 16 * 4096;

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
    memMap = (EFI_MEMORY_DESCRIPTOR*)Allocate(memoryMapSize + 4096, MemoryType::MemoryMap);
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

    char* kernelStackTop = (char*)(header->stack) + header->stackSize;

    __asm__ __volatile__ (
        ".intel_syntax noprefix;"
        "movq rbp, %2;"
        "movq rsp, %2;"
        "callq rax;"
        ".att_syntax prefix"
        : : "D"(header), "a"(kernelMain), "r"(kernelStackTop)
    );
}