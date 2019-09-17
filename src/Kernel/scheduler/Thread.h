#pragma once

#include "types.h"
#include "interrupts/IDT.h"

constexpr uint64 KernelStackPages = 3;
constexpr uint64 KernelStackSize = KernelStackPages * 4096;
constexpr uint64 UserStackPages = 4;
constexpr uint64 UserStackSize = UserStackPages * 4096;

struct ThreadInfo;

struct ThreadBlockEvent {
    enum {
        TYPE_NONE,
        TYPE_WAIT,
        TYPE_QUEUE_LOCK,
        TYPE_TRANSFER,
    } type;

    union {
        struct {
            uint64 remainingTicks;
        } wait;
        struct {
            uint64 coreID;
        } transfer;
    };
};

struct ThreadMemSpace {
    Atomic<uint64> refCount;
    uint64 pml4Entry;
};

struct ThreadFileDescriptor {
    int64 id;
    uint64 sysDesc;
};

struct ThreadFileDescriptors {
    Atomic<uint64> refCount;

    StickyLock lock;
    ktl::vector<ThreadFileDescriptor> fds;
};

struct ThreadGroup {
    ThreadInfo* owner;
    
};

struct ThreadInfo {
    ThreadInfo* next;
    ThreadInfo* prev;

    ThreadInfo* parent;

    Atomic<uint64> refCount;

    uint64 tid;
    
    uint64 coreID;
    
    bool exited;
    int64 exitCode;
    
    ThreadMemSpace* memSpace;
    ThreadFileDescriptors* fds;

    ktl::vector<ThreadInfo*> children; // may only be modified by the thread itself

    User* user;

    ThreadBlockEvent blockEvent;
    
    uint64 kernelStack;
    uint64 userGSBase;
    uint64 userFSBase;

    uint64 stickyCount;
    uint64 cliCount;

    bool unkillable;
    bool killPending;
    uint64 killCode;

    uint64 pageFaultRip;

    IDT::Registers registers;

    bool hasFPUBlock;
    char fpuState[] __attribute__((aligned(64)));   // buffer used to save and restore the SSE and FPU state of the thread
};