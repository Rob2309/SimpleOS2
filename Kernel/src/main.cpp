#include <KernelHeader.h>

#include "terminal.h"
#include "conio.h"
#include "PhysicalMemoryManager.h"
#include "VirtualMemoryManager.h"
#include "GDT.h"
#include "IDT.h"

#include "APIC.h"

extern uint64 g_APICBase;

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    
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
    VirtualMemoryManager::Init(info);

    printf("Hello World from virtual memory\n");

    while(true);

}