#pragma once

#include "types.h"
#include "interrupts/IDT.h"
#include "stl/list.h"
#include "Mutex.h"

constexpr uint64 KernelStackPages = 3;
constexpr uint64 KernelStackSize = KernelStackPages * 4096;
constexpr uint64 UserStackPages = 4;
constexpr uint64 UserStackSize = UserStackPages * 4096;

struct ThreadBlockEvent {
    enum {
        TYPE_NONE,
        TYPE_WAIT,
        TYPE_MUTEX,
        TYPE_NODE_READ,
        TYPE_NODE_WRITE,
    } type;

    union {
        struct {
            uint64 remainingMillis;
        } wait;
        struct {
            Mutex* lock;
        } mutex;
        struct {
            uint64 nodeID;
        } node;
    };
};

struct ProcessInfo;

struct ThreadInfo {
    ThreadInfo* next;
    ThreadInfo* prev;

    uint64 tid;
    ProcessInfo* process;

    ThreadBlockEvent blockEvent;
    
    uint64 kernelStack;
    uint64 userGSBase;

    IDT::Registers registers;
};

struct ProcessInfo {
    uint64 pid;

    uint64 pml4Entry;

    std::nlist<ThreadInfo> threads;
};