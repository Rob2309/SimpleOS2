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

#include "Ramdisk.h"

#include "KernelHeap.h"

#include "VFS.h"
#include "RamDevice.h"

uint64 g_TimeCounter = 0;

void TimerEvent(IDT::Registers* regs)
{
    g_TimeCounter += 10;
    Scheduler::Tick(regs);
}

void SyscallInterrupt(IDT::Registers* regs)
{
    switch(regs->rax) {
    case Syscall::FunctionGetPID: regs->rax = Scheduler::GetCurrentPID(); break;
    case Syscall::FunctionPrint: printf("%i: %s", Scheduler::GetCurrentPID(), (char*)(regs->rdi)); break;
    case Syscall::FunctionWait: Scheduler::ProcessWait(regs->rdi); Scheduler::Tick(regs); break;
    case Syscall::FunctionExit: Scheduler::ProcessExit(regs->rdi, regs); Scheduler::Tick(regs); break;
    case Syscall::FunctionFork: Scheduler::ProcessFork(regs); break;
    }
}

static void SetupTestProcess(uint8* loadBase)
{
    File f = Ramdisk::GetFileData("test.elf");

    uint64 pages = (GetELFSize(f.data) + 4095) / 4096;
    uint64 pml4Entry = MemoryManager::CreateProcessMap();
    MemoryManager::SwitchProcessMap(pml4Entry);

    uint8* processBuffer = (uint8*)MemoryManager::AllocatePages(pages);
    for(uint64 i = 0; i < pages; i++) {
        MemoryManager::MapProcessPage(pml4Entry, processBuffer + i * 4096, loadBase + i * 4096);
    }

    Elf64Addr entryPoint;
    if(!PrepareELF(f.data, loadBase, &entryPoint)) {
        printf("Failed to setup process\n");
        return;
    }

    uint8* stack = (uint8*)MemoryManager::AllocatePages(10);
    for(int i = 0; i < 10; i++)
        MemoryManager::MapProcessPage(pml4Entry, stack + i * 4096, (void*)(0x1000 + i * 4096));

    Scheduler::RegisterProcess(pml4Entry, 0x1000 + 10 * 4096, entryPoint, true);

    delete[] f.data;
}

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    Terminal::Init(info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth, info->screenColorsInverted);
    Terminal::Clear();

    SetTerminalColor(180, 180, 180);
    printf("Kernel at 0x%x\n", info->kernelImage.buffer);
    
    MemoryManager::Init(info);

    GDT::Init();
    IDT::Init();
    IDT::SetISR(Syscall::InterruptNumber, SyscallInterrupt);

    VFS::Init();
    VFS::CreateFolder("/", "dev");

    RamDevice* initrdDev = new RamDevice("ram0", info->ramdiskImage.buffer, info->ramdiskImage.numPages * 4096);
    Ramdisk::Init("/dev/ram0");

    APIC::Init();
    APIC::SetTimerEvent(TimerEvent);

    SetupTestProcess((uint8*)0x16000);

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