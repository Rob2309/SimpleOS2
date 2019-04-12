#pragma once

#include "types.h"
#include "IDT.h"
#include "list.h"
#include "Mutex.h"

constexpr uint64 KernelStackPages = 3;
constexpr uint64 KernelStackSize = KernelStackPages * 4096;

struct ThreadBlockEvent {
    enum {
        TYPE_NONE,
        TYPE_WAIT,
    } type;

    union {
        struct {
            uint64 remainingMillis;
        } wait;
    };
};

struct ProcessInfo;

struct ThreadInfo {
    uint64 tid;
    ProcessInfo* process;

    ThreadBlockEvent blockEvent;
    
    uint64 kernelStack;
    uint64 userGSBase;

    IDT::Registers registers;
};

struct ProcessInfo {
    uint64 pid;

    Mutex lock;

    uint64 pml4Entry;

    std::list<ThreadInfo*> threads;
};