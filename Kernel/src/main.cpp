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

    __asm__ __volatile__ (
        "mov $0x23, %%rax;" // 0x23
        "mov %%ax, %%ds;"
        "mov %%ax, %%es;"
        "mov %%ax, %%fs;"
        "mov %%ax, %%gs;"
        "mov %%rsp, %%rax;"
        "push $0x23;"
        "push %%rax;"
        "pushfq;"
        "push $0x1B;"           // 0x1B
        "leaq 1f(%%rip), %%rax;"
        "push %%rax;"
        "iretq;"
        "1: nop"
        : : : "rax"
    );

    printf("Hello from usermode!\n");

    while(true);

}