#pragma once

#include "types.h"
#include "IDT.h"
#include "SyscallHandler.h"

struct FileDescriptor
{
    uint64 id;
    uint64 node;

    bool read;
    bool write;
};

namespace Scheduler {

    constexpr uint64 KernelStackSize = 3 * 4096;

    uint64 RegisterProcess(uint64 pml4Entry, uint64 rsp, uint64 rip, bool user, uint64 kernelStack);

    void Start();

    void Tick(IDT::Registers* regs);

    void ProcessWait(uint64 ms);
    void ProcessExit(uint64 code, IDT::Registers* regs);
    void ProcessFork(IDT::Registers* regs);

    void ProcessYield();

    uint64 ProcessAddFileDesc(uint64 node, bool read, bool write);
    void ProcessRemoveFileDesc(uint64 desc);
    FileDescriptor* ProcessGetFileDesc(uint64 id);

    uint64 GetCurrentPID();

}