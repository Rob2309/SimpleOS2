#pragma once

#include "types.h"
#include "interrupts/IDT.h"
#include "ktl/vector.h"
#include "user/User.h"
#include "atomic/Atomics.h"
#include "locks/StickyLock.h"

constexpr uint64 KernelStackPages = 3;
constexpr uint64 KernelStackSize = KernelStackPages * 4096;
constexpr uint64 UserStackPages = 4;
constexpr uint64 UserStackSize = UserStackPages * 4096;

struct ThreadInfo;

struct ThreadState {
    enum {
        STATE_READY,

        STATE_WAIT,
        STATE_QUEUE_LOCK,
        STATE_JOIN,

        STATE_EXITED,
        STATE_DEAD,
    } type;

    union {
        struct {
            uint64 remainingTicks;
        } wait;
        struct {
            ThreadInfo* joinThread;
        } join;
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

struct ThreadInfo {
    ThreadInfo* next;
    ThreadInfo* prev;

    ktl::vector<ThreadInfo*> joinThreads;   // threads this thread is supposed to free

    uint64 tid;
    
    int64 exitCode;
    bool killPending;                       // Set this flag to inform a thread that it should kill itself
    
    ThreadMemSpace* memSpace;
    ThreadFileDescriptors* fds;

    User* user;

    ThreadState state;
    
    uint64 kernelStack;
    uint64 userGSBase;
    uint64 userFSBase;

    uint64 stickyCount;
    uint64 cliCount;

    uint64 pageFaultRip;

    IDT::Registers registers;

    bool hasFPUBlock;
    char* fpuBuffer;   // buffer used to save and restore the SSE and FPU state of the thread
};