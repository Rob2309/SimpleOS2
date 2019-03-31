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

    constexpr uint64 ControlFuncWait = 1;
    constexpr uint64 ControlFuncExit = 2;
    constexpr uint64 ControlFuncFork = 3;

    constexpr uint64 KernelStackSize = 3 * 4096;

    uint64 RegisterProcess(uint64 pml4Entry, uint64 rsp, uint64 rip, bool user, uint64 kernelStack);

    void Start();

    void Tick(IDT::Registers* regs);

    void ProcessWait(uint64 ms);
    void ProcessExit(uint64 code);
    inline uint64 __attribute__((always_inline)) ProcessFork()
    {
        uint64 ret;
        __asm__ __volatile__ (
            "int $127"
            : "=a"(ret)
            : "a"(ControlFuncFork)
        );
        return ret;
    }
    void ProcessYield();

    uint64 ProcessAddFileDesc(uint64 node, bool read, bool write);
    void ProcessRemoveFileDesc(uint64 desc);
    FileDescriptor* ProcessGetFileDesc(uint64 id);

    uint64 GetCurrentPID();

}