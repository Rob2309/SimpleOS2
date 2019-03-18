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
#include "ZeroDevice.h"

uint64 g_TimeCounter = 0;

void TimerEvent(IDT::Registers* regs)
{
    g_TimeCounter += 10;
    Scheduler::Tick(regs, false);
}

void SyscallInterrupt(IDT::Registers* regs)
{
    switch(regs->rax) {
    case Syscall::FunctionGetPID: regs->rax = Scheduler::GetCurrentPID(); break;
    case Syscall::FunctionPrint: printf("%i: %s", Scheduler::GetCurrentPID(), (char*)(regs->rdi)); break;
    case Syscall::FunctionWait: Scheduler::ProcessWait(regs->rdi); Scheduler::Tick(regs, true); break;
    case Syscall::FunctionExit: Scheduler::ProcessExit(regs->rdi, regs); Scheduler::Tick(regs, false); break;
    case Syscall::FunctionFork: Scheduler::ProcessFork(regs); break;

    case Syscall::FunctionOpen: {
        uint64 node = VFS::GetFileNode((const char*)regs->rdi);
        if(node == 0) {
            regs->rax = 0;
            break;
        }
        if(!VFS::AddFileUserRead(node)) {
            regs->rax = 0;
            break;
        }
        regs->rax = Scheduler::ProcessAddFileDesc(node, true, false);
    } break;

    case Syscall::FunctionClose: {
        FileDescriptor* desc = Scheduler::ProcessGetFileDesc(regs->rdi);
        if(desc == nullptr)
            break;

        VFS::RemoveFileUserRead(desc->node);
        Scheduler::ProcessRemoveFileDesc(desc->id);
    } break;

    case Syscall::FunctionRead: {
        FileDescriptor* desc = Scheduler::ProcessGetFileDesc(regs->rsi);
        if(desc == nullptr) {
            regs->rax = 0;
            break;
        }

        regs->rax = VFS::ReadFile(desc->node, regs->r11, (void*)regs->rdi, regs->r10);
    } break;

    case Syscall::FunctionWrite: {
        FileDescriptor* desc = Scheduler::ProcessGetFileDesc(regs->rdi);
        if(desc == nullptr) {
            break;
        }

        VFS::WriteFile(desc->node, regs->r11, (void*)regs->rsi, regs->r10);
    } break;
    }
}

static void SetupTestProcess(uint8* loadBase)
{
    uint64 file = VFS::GetFileNode("/initrd/test.elf");
    if(file == 0) {
        printf("Failed to find test.elf\n");
        return;
    }
    if(!VFS::AddFileUserRead(file)) {
        printf("Failed to open test.elf\n");
        return;
    }

    uint64 fileSize = VFS::GetFileSize(file);
    uint8* fileBuffer = new uint8[fileSize];
    VFS::ReadFile(file, 0, fileBuffer, fileSize);
    VFS::RemoveFileUserRead(file);

    uint64 pages = (GetELFSize(fileBuffer) + 4095) / 4096;
    uint64 pml4Entry = MemoryManager::CreateProcessMap();
    MemoryManager::SwitchProcessMap(pml4Entry);

    uint8* processBuffer = (uint8*)MemoryManager::AllocatePages(pages);
    for(uint64 i = 0; i < pages; i++) {
        MemoryManager::MapProcessPage(pml4Entry, processBuffer + i * 4096, loadBase + i * 4096);
    }

    Elf64Addr entryPoint;
    if(!PrepareELF(fileBuffer, loadBase, &entryPoint)) {
        printf("Failed to setup process\n");
        return;
    }

    uint8* stack = (uint8*)MemoryManager::AllocatePages(10);
    for(int i = 0; i < 10; i++)
        MemoryManager::MapProcessPage(pml4Entry, stack + i * 4096, (void*)(0x1000 + i * 4096));

    Scheduler::RegisterProcess(pml4Entry, 0x1000 + 10 * 4096, entryPoint, true);

    delete[] fileBuffer;
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

    if(!VFS::CreateFolder("/", "dev"))
        printf("Failed to create /dev folder\n");
    if(!VFS::CreateFolder("/", "initrd"))
        printf("Failed to create /initrd folder\n");

    new RamDevice("ram0", info->ramdiskImage.buffer, info->ramdiskImage.numPages * 4096);
    RamdiskFS* ramfs = new RamdiskFS("/dev/ram0");
    VFS::Mount("/initrd", ramfs);

    new ZeroDevice("zero");

    APIC::Init();
    APIC::SetTimerEvent(TimerEvent);

    SetupTestProcess((uint8*)0x16000);
    //SetupTestProcess((uint8*)0x16000);
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