#include "KernelHeader.h"

#include "terminal/terminal.h"
#include "klib/stdio.h"
#include "arch/GDT.h"
#include "interrupts/IDT.h"
#include "arch/APIC.h"
#include "memory/MemoryManager.h"
#include "arch/SMP.h"

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    Terminal::Init(info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth, info->screenColorsInverted);
    Terminal::Clear();

    klog_info("Kernel", "Kernel at 0x%x", info->kernelImage.buffer);

    MemoryManager::Init(info);
    GDT::Init();
    IDT::Init();
    APIC::Init();
    
    SMP::StartCores(info);

    while(true) ;
}