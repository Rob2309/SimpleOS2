#include <KernelHeader.h>

#include "terminal.h"
#include "conio.h"
#include "PhysicalMemoryManager.h"

static const char* g_MemoryDesc[] = {
    "EfiReservedMemoryType",
    "EfiLoaderCode",
    "EfiLoaderData",
    "EfiBootServicesCode",
    "EfiBootServicesData",
    "EfiRuntimeServicesCode",
    "EfiRuntimeServicesData",
    "EfiConventionalMemory",
    "EfiUnusableMemory",
    "EfiACPIReclaimMemory",
    "EfiACPIMemoryNVS",
    "EfiMemoryMappedIO",
    "EfiMemoryMappedIOPortSpace",
    "EfiPalCode",
    "EfiMaxMemoryType"
};

extern "C" void __attribute__((ms_abi)) __attribute__((noreturn)) main(KernelHeader* info) {
    
    uint32* fontBuffer = nullptr;
    for(int m = 0; m < info->numModules; m++) {
        if(info->modules[m].type == ModuleDescriptor::TYPE_RAMDISK_IMAGE)
            fontBuffer = (uint32*)info->modules[m].buffer;
    }
    Terminal::Init(fontBuffer, info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth);
    Terminal::Clear();
    
    PhysicalMemoryManager::Init(info);

    void* a = PhysicalMemoryManager::AllocatePages(10);
    void* b = PhysicalMemoryManager::AllocatePages(3);
    PhysicalMemoryManager::FreePages(a, 10);

    void* c = PhysicalMemoryManager::AllocatePages(11);
    void* d = PhysicalMemoryManager::AllocatePages(5);

    printf("A: 0x%x\n", a);
    printf("B: 0x%x\n", b);
    printf("C: 0x%x\n", c);
    printf("D: 0x%x\n", d);

    while(true);

}