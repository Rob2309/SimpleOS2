#include <KernelHeader.h>

#include "terminal.h"
#include "conio.h"
#include "MemoryManager.h"
#include "GDT.h"
#include "IDT.h"
#include "APIC.h"

#include "Scheduler.h"

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

    IDT::EnableInterrupts();
    APIC::StartTimer(1000);

    Scheduler::Start();

    // should never be reached
    while(true) {
        __asm__ __volatile__ (
            "hlt"
        );
    }

}