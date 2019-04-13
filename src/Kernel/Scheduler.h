#pragma once

#include "types.h"
#include "IDT.h"

namespace Scheduler {

    uint64 CreateProcess(uint64 pml4Entry, IDT::Registers* regs);
    uint64 CreateKernelThread(uint64 rip);

    void Start();

    void Tick(IDT::Registers* regs);

    void ThreadWait(uint64 ms, IDT::Registers* returnregs);
    void ThreadWaitForLock(void* lock, IDT::Registers* returnregs);
    void ThreadExit(uint64 code);
    uint64 ThreadCreateThread(uint64 entry, uint64 stack);
    uint64 ThreadGetTID();
    uint64 ThreadGetPID();

    void KernelThreadWait(uint64 ms);

}