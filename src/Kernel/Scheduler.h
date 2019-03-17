#pragma once

#include "types.h"
#include "IDT.h"

namespace Scheduler {

    uint64 RegisterProcess(uint64 pml4Entry, uint64 rsp, uint64 rip, bool user);

    void Start();
    void Tick(IDT::Registers* regs, bool processBlocked);

    void ProcessWait(uint64 ms);
    void ProcessExit(uint64 code, IDT::Registers* regs);
    void ProcessFork(IDT::Registers* regs);

    uint64 GetCurrentPID();

}