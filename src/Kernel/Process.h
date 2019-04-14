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
        TYPE_MUTEX,
    } type;

    union {
        struct {
            uint64 remainingMillis;
        } wait;
        struct {
            Mutex* lock;
        } mutex;
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