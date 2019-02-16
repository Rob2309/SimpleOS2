#pragma once

#include "types.h"
#include "IDT.h"

namespace Scheduler {

    void RegisterProcess(uint64 pml4Entry, uint64 rsp, uint64 rip, bool user);

    void Start();
    void Tick(IDT::Registers* regs);

    void ProcessWait(uint64 ms);

    uint64 GetCurrentPID();

}