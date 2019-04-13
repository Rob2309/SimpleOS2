#include "SyscallHandler.h"

#include "MSR.h"
#include "GDT.h"
#include "conio.h"

#include "Syscall.h"
#include "IDT.h"
#include "Scheduler.h"
#include "MemoryManager.h"
#include "memutil.h"

#include "Process.h"

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

        uint64 pml4Entry = MemoryManager::ForkProcessMap();

        return Scheduler::CreateProcess(pml4Entry, &childRegs);
    }

    static void DoWait(State* state, uint64 ms)
    {
        IDT::Registers returnregs;
        memset(&returnregs, 0, sizeof(IDT::Registers));
        returnregs.rip = state->userrip;
        returnregs.userrsp = state->userrsp;
        returnregs.rbp = state->userrbp;
        returnregs.rflags = state->userflags;
        returnregs.rbx = state->userrbx;
        returnregs.r12 = state->userr12;
        returnregs.r13 = state->userr13;
        returnregs.r14 = state->userr14;
        returnregs.r15 = state->userr15;
        returnregs.ss = GDT::UserData;
        returnregs.cs = GDT::UserCode;
        returnregs.ds = GDT::UserData;

        Scheduler::ThreadWait(ms, &returnregs);
    }

    static void DoWaitForLock(State* state, void* lock)
    {
        IDT::Registers returnregs;
        memset(&returnregs, 0, sizeof(IDT::Registers));
        returnregs.rip = state->userrip;
        returnregs.userrsp = state->userrsp;
        returnregs.rbp = state->userrbp;
        returnregs.rflags = state->userflags;
        returnregs.rbx = state->userrbx;
        returnregs.r12 = state->userr12;
        returnregs.r13 = state->userr13;
        returnregs.r14 = state->userr14;
        returnregs.r15 = state->userr15;
        returnregs.ss = GDT::UserData;
        returnregs.cs = GDT::UserCode;
        returnregs.ds = GDT::UserData;

        Scheduler::ThreadWaitForLock(lock, &returnregs);
    }

    extern "C" uint64 SyscallDispatcher(uint64 func, uint64 arg1, uint64 arg2, uint64 arg3, uint64 arg4, State* state)
    {
        switch(func) {
        case Syscall::FunctionPrint: printf((const char*)arg1); break;
        case Syscall::FunctionWait: DoWait(state, arg1); break;
        case Syscall::FunctionGetPID: return Scheduler::ThreadGetPID(); break;
        case Syscall::FunctionExit: Scheduler::ThreadExit(arg1); break;
        case Syscall::FunctionFork: return DoFork(state); break;
        case Syscall::FunctionCreateThread: Scheduler::ThreadCreateThread(arg1, arg2); break;
        case Syscall::FunctionWaitForLock: DoWaitForLock(state, (void*)arg1); break;
        }

        return 0;
    }

}