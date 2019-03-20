#include "SyscallHandler.h"

#include "Syscall.h"
#include "ISRNumbers.h"
#include "IDT.h"
#include "Scheduler.h"
#include "conio.h"
#include "VFS.h"

namespace SyscallHandler {
    
    static void SyscallInterruptHandler(IDT::Registers* regs)
    {
        IDT::EnableInterrupts();

        switch(regs->rax) {
        case Syscall::FunctionGetPID: regs->rax = Scheduler::GetCurrentPID(); break;
        case Syscall::FunctionPrint: printf("%i: %s", Scheduler::GetCurrentPID(), (char*)(regs->rdi)); break;

        case Syscall::FunctionWait: 
            IDT::DisableInterrupts();
            Scheduler::ProcessWait(regs->rdi); 
            Scheduler::Tick(regs, true);
            IDT::EnableInterrupts();
            break;
        case Syscall::FunctionExit: 
            IDT::DisableInterrupts();
            Scheduler::ProcessExit(regs->rdi, regs); 
            Scheduler::Tick(regs, false);
            IDT::EnableInterrupts();
            break;
        case Syscall::FunctionFork: 
            IDT::DisableInterrupts();
            Scheduler::ProcessFork(regs);
            IDT::EnableInterrupts();
            break;

        case Syscall::FunctionOpen: regs->rax = VFS::OpenFile((const char*)regs->rdi); break;
        case Syscall::FunctionClose: VFS::CloseFile(regs->rdi); break;
        case Syscall::FunctionRead: regs->rax = VFS::ReadFile(regs->rdi, (void*)regs->rsi, regs->r10); break;
        case Syscall::FunctionWrite: VFS::WriteFile(regs->rdi, (void*)regs->rsi, regs->r10); break;
        }
    }

    void Init() {
        IDT::SetISR(ISRNumbers::Syscall, SyscallInterruptHandler);
    }

}