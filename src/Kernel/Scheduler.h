#pragma once

#include "types.h"
#include "IDT.h"

namespace Scheduler {

    uint64 CreateProcess(uint64 pml4Entry, IDT::Registers* regs);
    uint64 CreateKernelThread(uint64 rip);
    uint64 CloneProcess(IDT::Registers* regs);

    void Start();

    void Tick(IDT::Registers* regs);

    void ThreadWait(uint64 ms);
    void ThreadWaitForLock(void* lock);
    void ThreadWaitForNodeRead(uint64 node);
    void ThreadWaitForNodeWrite(uint64 node);
    void ThreadExit(uint64 code);
    uint64 ThreadCreateThread(uint64 entry, uint64 stack);
    uint64 ThreadGetTID();
    uint64 ThreadGetPID();

    void NotifyNodeRead(uint64 nodeID);
    void NotifyNodeWrite(uint64 nodeID);

    uint64 ProcessAddFileDescriptor(uint64 nodeID, bool read, bool write);
    void ProcessRemoveFileDescriptor(uint64 id);
    uint64 ProcessGetFileDescriptorNode(uint64 id);

    void ProcessExec(uint64 pml4Entry, IDT::Registers* regs);

}