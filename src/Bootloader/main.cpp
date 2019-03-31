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

    // Get LoadedImageProtocol
    EFI_GUID guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_STATUS err = sysTable->BootServices->HandleProtocol(imgHandle, &guid, (void**)&EFIUtil::LoadedImage);
    if(err != EFI_SUCCESS) {
        Console::Print(L"Failed to init loaded image protocol\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    // Get Filesystem access of the device the bootloader is located on
    guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    err = sysTable->BootServices->HandleProtocol(EFIUtil::LoadedImage->DeviceHandle, &guid, (void**)&EFIUtil::FileSystem);
    if(err != EFI_SUCCESS) {
        Console::Print(L"Failed to init filesystem protocol\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    // Get Graphics Protocol of the screen
    guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    err = sysTable->BootServices->LocateProtocol(&guid, nullptr, (void**)&EFIUtil::Graphics);
    if(err != EFI_SUCCESS) {
        Console::Print(L"Failed to init grpahics protocol\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    Console::Print(L"Switching video mode...\r\n");

    // find best resolution the Graphics Protocol supports
    UINT32 resBestX = 0;
    UINT32 resBestY = 0;
    UINT32 resBestScanlineWidth = 0;
    UINT32 resBestMode = 0;
    for(UINT32 m = 0; m < EFIUtil::Graphics->Mode->MaxMode; m++) {
        UINTN infoSize;
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info;
        EFIUtil::Graphics->QueryMode(EFIUtil::Graphics, m, &infoSize, &info);

        // retrict to 1920x1080, as otherwise qemu tends to create huge windows that do not fit onto the screen
        if(info->VerticalResolution > resBestY && info->HorizontalResolution <= 1920 && info->VerticalResolution <= 1080 && (info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor || info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor)) {
            resBestX = info->HorizontalResolution;
            resBestY = info->VerticalResolution;
            resBestScanlineWidth = info->PixelsPerScanLine;
            resBestMode = m;
        }
    }

    EFIUtil::Graphics->SetMode(EFIUtil::Graphics, resBestMode);

    Console::Print(L"Loading modules...\r\n");

    // Allocate the Header that will be passed to the kernel
    KernelHeader* header = (KernelHeader*)Allocate(sizeof(KernelHeader), (EFI_MEMORY_TYPE)0x80000001);
    // Create paging structures to map the topmost 512GB to physical memory
    Paging::Init(header);
    // Convert the header pointer to a high-memory pointer
    header = (KernelHeader*)Paging::ConvertPtr(header);

    // Load Kernel elf file
    FileIO::FileData kernelData = FileIO::ReadFile(L"EFI\\BOOT\\kernel.sys");
    if(kernelData.size == 0) {
        Console::Print(L"Failed to load kernel image\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    // Load ramdisk image
    FileIO::FileData ramdiskData = FileIO::ReadFile(L"EFI\\BOOT\\initrd");
    if(ramdiskData.size == 0) {
        Console::Print(L"Failed to load ramdisk\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    Console::Print(L"Preparing kernel...\r\n");

    Elf64Addr kernelEntryPoint = 0;
    uint64 size = GetELFSize(kernelData.data);
    // allocate the buffer the relocated kernel image will be in
    uint8* processBuffer = (uint8*)Paging::ConvertPtr(Allocate(size, (EFI_MEMORY_TYPE)0x80000001));

    // relocate the kernel into the buffer
    if(!PrepareELF(kernelData.data, processBuffer, &kernelEntryPoint)) {
        Console::Print(L"Failed to prepare kernel\r\nPress any key to exit...\r\n");
        EFIUtil::WaitForKey();
        return EFI_LOAD_ERROR;
    }

    uint64* debugData = (uint64*)0x1000;
    err = EFIUtil::SystemTable->BootServices->AllocatePages(AllocateAddress, (EFI_MEMORY_TYPE)0x80000001, 1, (EFI_PHYSICAL_ADDRESS*)&debugData);
    if(err != EFI_SUCCESS) {
        Console::Print(L"Failed to prepare debugging information\r\nHalting here for manual debugging\r\n");
    }
    debugData[0] = GetELFTextAddr(kernelData.data, processBuffer);

    // the old buffer is not needed anymore
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

    // Allocate 16 pages as a kernel stack
    void* newStack = Paging::ConvertPtr(Allocate(16 * 4096, (EFI_MEMORY_TYPE)0x80000001));
    header->stack = newStack;
    header->stackPages = 16;

    Console::Print(L"Exiting Boot services and starting kernel...\r\nPress any key to continue...\r\n");
    //EFIUtil::WaitForKey();

    // We have to get the most recent memory map before calling ExitBootServices
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
        "movq rsp, %2;"             // switch stack to kernel stack
        "callq rax;"                // call the kernel
        ".att_syntax prefix"
        : : "D"(header), "a"(kernelMain), "r"(kernelStackTop)
    );
}