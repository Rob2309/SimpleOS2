#include <KernelHeader.h>

#include "terminal.h"
#include "conio.h"
#include "PhysicalMemoryManager.h"
#include "VirtualMemoryManager.h"
#include "GDT.h"
#include "IDT.h"

#include "APIC.h"

static void ISRHandlerTest(IDT::Registers* regs)
{
    //printf("TestInterrupt\n");
}

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

    GDT::Init();
    IDT::Init();
    IDT::EnableInterrupts();

    APIC::Init();
    APIC::StartTimer(128, 0xFFFFFF, true);

    IDT::SetISR(95, ISRHandlerTest);

    __asm__ __volatile__ (
        "cli;"
        "mov $0x23, %%rax;"
        "mov %%ax, %%ds;"
        "mov %%ax, %%es;"
        "mov %%ax, %%fs;"
        "mov %%ax, %%gs;"
        "push $0x23;"
        "push %%rsp;"
        "pushfq;"
        "push $0x1B;"
        "leaq 1f(%%rip), %%rax;"
        "push %%rax;"
        "iretq;"
        "1: add $4, %%rsp;"
        : : : "rax"
    );

    printf("Hello from usermode!\n");

    bool wait = true;
    while(wait);

    __asm__ __volatile__ (
        "int $95"
    );

    while(true);

}