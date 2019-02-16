#include <KernelHeader.h>

#include "terminal.h"
#include "conio.h"
#include "MemoryManager.h"
#include "GDT.h"
#include "IDT.h"
#include "APIC.h"

#include "Scheduler.h"

#include "ELF.h"

#include "Syscall.h"

uint64 g_TimeCounter = 0;

void TimerEvent(IDT::Registers* regs)
{
    g_TimeCounter += 10;
    Scheduler::Tick(regs);
}

void SyscallInterrupt(IDT::Registers* regs)
{
    switch(regs->rax) {
    case Syscall::FunctionPrint: printf((char*)(regs->rdi)); break;
    case Syscall::FunctionWait: Scheduler::ProcessWait(regs->rdi); Scheduler::Tick(regs); break;
    }
}

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    
    Terminal::Init((uint32*)info->fontImage.buffer, info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth, info->screenColorsInverted);
    Terminal::Clear();

    SetTerminalColor(180, 180, 180);
    printf("Kernel at 0x%x\n", info->kernelImage.buffer);
    
    MemoryManager::Init(info);

    GDT::Init();
    IDT::Init();
    IDT::SetISR(Syscall::InterruptNumber, SyscallInterrupt);
    
    uint64 pages = (GetELFSize(info->ramdiskImage.buffer) + 4095) / 4096;
    uint8* loadBase = (uint8*)0x16000;
    uint64 pml4Entry = MemoryManager::CreateProcessMap();
    MemoryManager::SwitchProcessMap(pml4Entry);

    uint8* processBuffer = (uint8*)MemoryManager::AllocatePages(pages);
    for(uint64 i = 0; i < pages; i++) {
        MemoryManager::MapProcessPage(processBuffer + i * 4096, loadBase + i * 4096);
    }

    Elf64Addr entryPoint;
    if(!PrepareELF(info->ramdiskImage.buffer, loadBase, &entryPoint)) {
        printf("Failed to setup process\n");
        while(true);
    }

    uint8* stack = (uint8*)MemoryManager::AllocatePages(10);
    for(int i = 0; i < 10; i++)
        MemoryManager::MapProcessPage(stack + i * 4096, (void*)(0x1000 + i * 4096));

    Scheduler::RegisterProcess(pml4Entry, 0x1000 + 10 * 4096, entryPoint, true);

    APIC::Init();
    APIC::SetTimerEvent(TimerEvent);

    IDT::EnableInterrupts();
    APIC::StartTimer(10);

    Scheduler::Start();

    // should never be reached
    while(true) {
        __asm__ __volatile__ (
            "hlt"
        );
    }

}