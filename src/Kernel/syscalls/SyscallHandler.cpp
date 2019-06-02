#include "SyscallHandler.h"

#include "arch/MSR.h"
#include "arch/GDT.h"
#include "terminal/conio.h"
#include "terminal/terminal.h"

#include "Syscall.h"
#include "interrupts/IDT.h"
#include "scheduler/Scheduler.h"
#include "memory/MemoryManager.h"
#include "memory/memutil.h"

#include "scheduler/Process.h"

#include "fs/VFS.h"

#include "scheduler/ELF.h"

namespace SyscallHandler {

    extern "C" void SyscallEntry();

    void Init()
    {
        uint64 eferVal = MSR::Read(MSR::RegEFER);
        eferVal |= 1;
        MSR::Write(MSR::RegEFER, eferVal);

        uint64 starVal = ((uint64)(GDT::UserCode - 16) << 48) | ((uint64)GDT::KernelCode << 32);
        MSR::Write(MSR::RegSTAR, starVal);

        uint64 lstarVal = (uint64)&SyscallEntry;
        MSR::Write(MSR::RegLSTAR, lstarVal);

        uint64 cstarVal = 0;
        MSR::Write(MSR::RegCSTAR, cstarVal);

        uint64 sfmaskVal = 0b000000000001000000000;     // disable interrupts on syscall
        MSR::Write(MSR::RegSFMASK, sfmaskVal);
    }

    struct __attribute__((packed)) State
    {
        uint64 userrip, userrsp, userrbp, userflags;
        uint64 userrbx, userr12, userr13, userr14, userr15;
    };

    static uint64 DoFork(State* state)
    {
        IDT::Registers childRegs;
        memset(&childRegs, 0, sizeof(IDT::Registers));
        childRegs.rip = state->userrip;
        childRegs.userrsp = state->userrsp;
        childRegs.rbp = state->userrbp;
        childRegs.rflags = state->userflags;
        childRegs.rbx = state->userrbx;
        childRegs.r12 = state->userr12;
        childRegs.r13 = state->userr13;
        childRegs.r14 = state->userr14;
        childRegs.r15 = state->userr15;
        childRegs.ss = GDT::UserData;
        childRegs.cs = GDT::UserCode;
        childRegs.ds = GDT::UserData;
        childRegs.rax = 0;

        return Scheduler::CloneProcess(&childRegs);
    }

    extern "C" uint64 SyscallDispatcher(uint64 func, uint64 arg1, uint64 arg2, uint64 arg3, uint64 arg4, State* state)
    {
        switch(func) {
        case Syscall::FunctionPrint:
            SetTerminalColor(200, 50, 50);
            printf("[%i.%i] ", Scheduler::ThreadGetPID(), Scheduler::ThreadGetTID());
            SetTerminalColor(255, 255, 255);
            printf((const char*)arg1); 
            break;
        case Syscall::FunctionWait: Scheduler::ThreadWait(arg1); break;
        case Syscall::FunctionGetPID: return Scheduler::ThreadGetPID(); break;
        case Syscall::FunctionExit: 
            SetTerminalColor(200, 50, 50);
            printf("[%i.%i] ", Scheduler::ThreadGetPID(), Scheduler::ThreadGetTID());
            SetTerminalColor(255, 255, 255);
            printf("Exiting with code %i\n", arg1);
            Scheduler::ThreadExit(arg1); 
            break;
        case Syscall::FunctionFork: return DoFork(state); break;
        case Syscall::FunctionCreateThread: Scheduler::ThreadCreateThread(arg1, arg2); break;
        case Syscall::FunctionWaitForLock: Scheduler::ThreadWaitForLock(MemoryManager::UserToKernelPtr((void*)arg1)); break;

        case Syscall::FunctionCreateFile: return VFS::CreateFile((const char*)arg1); break;
        case Syscall::FunctionCreateFolder: return VFS::CreateFolder((const char*)arg1); break;
        case Syscall::FunctionOpen: {
                uint64 sysDesc = VFS::Open((const char*)arg1);
                if(sysDesc == 0)
                    return 0;
                uint64 desc = Scheduler::ProcessAddFileDescriptor(sysDesc);
                return desc;
            } break;
        case Syscall::FunctionClose: Scheduler::ProcessCloseFileDescriptor(arg1); break;
        case Syscall::FunctionRead: return VFS::Read(arg1, (void*)arg2, arg3); break;
        case Syscall::FunctionWrite: return VFS::Write(arg1, (const void*)arg2, arg3); break;
        }

        return 0;
    }

}