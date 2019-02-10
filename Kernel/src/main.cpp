#include <KernelHeader.h>

#include "terminal.h"
#include "conio.h"
#include "PhysicalMemoryManager.h"
#include "VirtualMemoryManager.h"
#include "GDT.h"
#include "IDT.h"

#include "APIC.h"

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    
    Terminal::Init((uint32*)info->fontImage.buffer, info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth);
    Terminal::Clear();

    printf("Kernel at 0x%x\n", info->kernelImage.buffer);
    
    PhysicalMemoryManager::Init(info);
    VirtualMemoryManager::Init(info);

    GDT::Init();
    IDT::Init();
    APIC::Init();

    IDT::EnableInterrupts();

    

    while(true);

}