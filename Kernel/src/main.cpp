#include <KernelHeader.h>

#include "terminal.h"
#include "conio.h"
#include "PhysicalMemoryManager.h"
#include "GDT.h"
#include "IDT.h"

extern uint64 g_APICBase;

extern "C" void __attribute__((ms_abi)) __attribute__((noreturn)) main(KernelHeader* info) {
    
    uint32* fontBuffer = nullptr;
    for(int m = 0; m < info->numModules; m++) {
        if(info->modules[m].type == ModuleDescriptor::TYPE_RAMDISK_IMAGE)
            fontBuffer = (uint32*)info->modules[m].buffer;
    }
    Terminal::Init(fontBuffer, info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth);
    Terminal::Clear();

    for(int m = 0; m < info->numModules; m++) {
        if(info->modules[m].type == ModuleDescriptor::TYPE_ELF_IMAGE)
            printf("Kernel at 0x%x\n", info->modules[m].buffer);
    }
    
    PhysicalMemoryManager::Init(info);

    GDT::Init();

    IDT::Init();

    uint32 eax;
    uint32 edx;
    __asm__ __volatile__ (
        "movl $0x1B, %%ecx;"
        "rdmsr"
        : "=a" (eax), "=d" (edx)
        :
        : "ecx"
    );

    g_APICBase = (edx << 32) | (eax & 0xFFFFF000);
    printf("APIC Base: 0x%x\n", g_APICBase);

    *(uint32*)(g_APICBase + 0xF0) = 0x100 | 101;

    *(uint32*)(g_APICBase + 0x370) = 0x10000 | 102;

    *(uint32*)(g_APICBase + 0x3E0) = 0b1010;
    *(uint32*)(g_APICBase + 0x320) = 0x20000 | 100;
    *(uint32*)(g_APICBase + 0x380) = 0xFFFFFF;

    while(true);

}