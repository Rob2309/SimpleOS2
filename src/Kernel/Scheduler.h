#pragma once

#include "types.h"
#include "IDT.h"

namespace Scheduler {

    uint64 RegisterProcess(uint64 pml4Entry, uint64 kernelStack, IDT::Registers* regs);
    uint64 RegisterProcess(uint64 pml4Entry, uint64 rsp, uint64 rip, bool user, uint64 kernelStack);
    
    uint64 CreateKernelThread(uint64 rip);

    void Start();

    void Tick(IDT::Registers* regs);

    void ProcessWait(uint64 ms, IDT::Registers* returnregs);
    void ProcessExit(uint64 code);

    uint64 GetCurrentPID();

}