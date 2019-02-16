#include <KernelHeader.h>

#include "terminal.h"
#include "conio.h"
#include "MemoryManager.h"
#include "GDT.h"
#include "IDT.h"

#include "APIC.h"

#include "Scheduler.h"

uint8 stack1[4096];
uint8 stack2[4096];

void Process1()
{
    printf("Process1\n");
    while(true);
}

void Process2()
{
    printf("Process2\n");
    while(true);
}

void TimerEvent(IDT::Registers* regs)
{
    Scheduler::Tick(regs);
}

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    
    Terminal::Init((uint32*)info->fontImage.buffer, info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth, info->screenColorsInverted);
    Terminal::Clear();

    SetTerminalColor(180, 180, 180);
    printf("Kernel at 0x%x\n", info->kernelImage.buffer);
    
    MemoryManager::Init(info);

    GDT::Init();
    IDT::Init();
    
    APIC::Init();
    APIC::SetTimerEvent(TimerEvent);

    Scheduler::RegisterProcess(0, (uint64)&stack1[4096], (uint64)&Process1, false);
    Scheduler::RegisterProcess(0, (uint64)&stack2[4096], (uint64)&Process2, false);

    IDT::EnableInterrupts();
    APIC::StartTimer(1000);

    while(true);

}