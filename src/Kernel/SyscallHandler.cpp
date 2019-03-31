#include "SyscallHandler.h"

#include "MSR.h"
#include "GDT.h"
#include "conio.h"

#include "Syscall.h"
#include "IDT.h"
#include "Scheduler.h"

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

        uint64 sfmaskVal = 0;//0b000000000001000000000;
        MSR::Write(MSR::RegSFMASK, sfmaskVal);
    }

    struct __attribute__((packed)) Regs {
        uint64 rax, rbx, rdx, rdi, rsi, rbp, r8, r9, r12, r13, r14, r15;
        uint64 returnrsp, returnrflags, returnrip;
    };

    extern "C" uint64 SyscallDispatcher(uint64 func, uint64 arg1, uint64 arg2, uint64 arg3, uint64 arg4)
    {
        switch(func) {
        case Syscall::FunctionPrint: printf((const char*)arg1); break;
        case Syscall::FunctionWait: Scheduler::ProcessWait(arg1); break;
        case Syscall::FunctionGetPID: return Scheduler::GetCurrentPID(); break;
        case Syscall::FunctionExit: Scheduler::ProcessExit(arg1); break;
        case Syscall::FunctionFork: return Scheduler::ProcessFork(); break;
        }

        return 0;
    }

}