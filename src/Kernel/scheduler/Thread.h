#pragma once

#include "types.h"
#include "interrupts/IDT.h"
#include "ktl/vector.h"
#include "atomic/Atomics.h"
#include "locks/StickyLock.h"
#include "ktl/AnchorList.h"

constexpr uint64 KernelStackPages = 3;
constexpr uint64 KernelStackSize = KernelStackPages * 4096;
constexpr uint64 UserStackPages = 4;
constexpr uint64 UserStackSize = UserStackPages * 4096;

struct ThreadInfo;

struct ThreadState {
    enum Type {
        READY,

        SLEEP,
        QUEUE_LOCK,
        JOIN,

        FINISHED,
        EXITED,
    } type;

    uint64 arg;
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
    ktl::Anchor<ThreadInfo> activeListAnchor;
    ktl::Anchor<ThreadInfo> globalListAnchor;
    ktl::Anchor<ThreadInfo> joinListAnchor;

    ThreadInfo* mainThread;                     // never changes, safe to access lockless

    StickyLock childThreadsLock;
    ktl::AnchorList<ThreadInfo, &ThreadInfo::joinListAnchor> childThreads;

    int64 tid;
    
    int64 exitCode;

    bool killPending;                       // Set this flag to inform a thread that it should kill itself
    
    ThreadMemSpace* memSpace;
    ThreadFileDescriptors* fds;

    uint64 uid;
    uint64 gid;

    ThreadState state;
    
    uint64 kernelStack;
    uint64 userGSBase;
    uint64 userFSBase;

    uint64 stickyCount;
    uint64 cliCount;

    uint64 faultRip;

    IDT::Registers registers;

    char* fpuBuffer;   // buffer used to save and restore the SSE and FPU state of the thread
};