#include <efi.h>

#include "efiutil.h"
#include "conio.h"
#include "file.h"
#include "KernelHeader.h"
#include "ELF.h"
#include "allocator.h"

#include "paging.h"

#include "physicalMap.h"

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

    KernelHeader* header = (KernelHeader*)Allocate(sizeof(KernelHeader), (EFI_MEMORY_TYPE)0x80000001);
    Paging::Init(header);
    header = (KernelHeader*)Paging::ConvertPtr(header);

    FileIO::FileData kernelData = FileIO::ReadFile(L"EFI\\BOOT\\kernel.sys");
    if(kernelData.size == 0) {
        Console::Print(L"Failed to load kernel image\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    FileIO::FileData ramdiskData = FileIO::ReadFile(L"EFI\\BOOT\\initrd");
    if(ramdiskData.size == 0) {
        Console::Print(L"Failed to load ramdisk\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    Console::Print(L"Preparing kernel...\r\n");

    Elf64Addr kernelEntryPoint = 0;
    uint64 size = GetELFSize(kernelData.data);
    uint8* processBuffer = (uint8*)Paging::ConvertPtr(Allocate(size, (EFI_MEMORY_TYPE)0x80000001));

    if(!PrepareELF(kernelData.data, processBuffer, &kernelEntryPoint)) {
        Console::Print(L"Failed to prepare kernel\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    Free(kernelData.data, kernelData.size);

    header->kernelImage.buffer = processBuffer;
    header->kernelImage.numPages = (size + 4095) / 4096;

    header->ramdiskImage.buffer = (uint8*)Paging::ConvertPtr(ramdiskData.data);
    header->ramdiskImage.numPages = (ramdiskData.size + 4095) / 4096;

    if(kernelEntryPoint == 0) {
        Console::Print(L"Kernel entry point not found\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    header->screenWidth = resBestX;
    header->screenHeight = resBestY;
    header->screenScanlineWidth = resBestScanlineWidth;
    header->screenBuffer = (uint32*)Paging::ConvertPtr((void*)EFIUtil::Graphics->Mode->FrameBufferBase);
    header->screenBufferPages = (EFIUtil::Graphics->Mode->FrameBufferSize + 4095) / 4096;
    header->screenColorsInverted = EFIUtil::Graphics->Mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor;

    void* newStack = Paging::ConvertPtr(Allocate(16 * 4096, (EFI_MEMORY_TYPE)0x80000001));
    header->stack = newStack;
    header->stackPages = 16;

    Console::Print(L"Exiting Boot services and starting kernel...\r\nPress any key to continue...\r\n");
    EFIUtil::WaitForKey();

    EfiMemoryMap memMap = EFIUtil::GetMemoryMap();
    err = EFIUtil::SystemTable->BootServices->ExitBootServices(EFIUtil::LoadedImage, memMap.key);
    if(err != EFI_SUCCESS) {
        Console::Print(L"Failed to exit boot services\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    header->physMapStart = PhysicalMap::Build(memMap);
    header->physMapEnd = header->physMapStart;
    while(header->physMapEnd->next != nullptr)
        header->physMapEnd = header->physMapEnd->next;

    typedef void (*KernelMain)(KernelHeader* header);
    KernelMain kernelMain = (KernelMain)kernelEntryPoint;
    char* kernelStackTop = (char*)(header->stack) + header->stackPages * 4096;

    __asm__ __volatile__ (
        ".intel_syntax noprefix;"
        "movq rbp, %2;"
        "movq rsp, %2;"
        "callq rax;"
        ".att_syntax prefix"
        : : "D"(header), "a"(kernelMain), "r"(kernelStackTop)
    );
}