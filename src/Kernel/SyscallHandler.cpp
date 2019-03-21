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

    void Init() {
        IDT::SetISR(ISRNumbers::Syscall, SyscallInterruptHandler);
    }

}