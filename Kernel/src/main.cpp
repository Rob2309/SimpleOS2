#include <KernelHeader.h>

#include "terminal.h"
#include "conio.h"
#include "PhysicalMemoryManager.h"
#include "VirtualMemoryManager.h"
#include "GDT.h"
#include "IDT.h"

#include "APIC.h"

#include "ELF.h"

#include "Syscall.h"

void SystemCallISR(IDT::Registers* regs)
{
    if(regs->rax == Syscall::FunctionPrint)
    {
        const char* str = (const char*)(regs->rdi);
        printf(str);
    } else {
        printf("Unknown func: %i\n", regs->rax);
    }
}

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    
    Terminal::Init((uint32*)info->fontImage.buffer, info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth);
    Terminal::Clear();

    printf("Kernel at 0x%x\n", info->kernelImage.buffer);
    
    PhysicalMemoryManager::Init(info);
    VirtualMemoryManager::Init(info);

    GDT::Init();
    IDT::Init();
    APIC::Init();

    uint8* programDiskImg = info->helloWorldImage.buffer;
    uint64 processSize = GetELFSize(programDiskImg);

    uint8* processImg = (uint8*)PhysicalMemoryManager::AllocatePages((processSize + 4095) / 4096);
    for(uint64 p = 0; p < (processSize + 4095) / 4096; p++)
        VirtualMemoryManager::MapPage((uint64)processImg + p * 4096, 0xFFFFFF0000000000 + p * 4096, true);
    processImg = (uint8*)0xFFFFFF0000000000;

    Elf64Addr entryPoint;
    if(!PrepareELF(programDiskImg, processImg, &entryPoint)) {
        printf("Failed to prepare test program\n");
    }

    uint8* processStack = (uint8*)PhysicalMemoryManager::AllocatePage();
    VirtualMemoryManager::MapPage((uint64)processStack, 0xFFFFFFFF00000000, true);
    processStack = (uint8*)(0xFFFFFFFF00001000);

    IDT::SetISR(0x80, SystemCallISR);

    __asm__ __volatile__ (
        "pushq $0x23;"
        "pushq %0;"
        "pushfq;"
        "pushq $0x1B;"
        "pushq %1;"
        "mov $0x23, %%rax;"
        "mov %%ax, %%ds;"
        "mov %%ax, %%es;"
        "mov %%ax, %%fs;"
        "mov %%ax, %%gs;"
        "iretq;"
        : : "r"(processStack), "r"(entryPoint)
    );

    while(true);

}