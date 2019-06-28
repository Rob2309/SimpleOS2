#pragma once

#include "types.h"
#include "interrupts/IDT.h"

constexpr uint64 KernelStackPages = 3;
constexpr uint64 KernelStackSize = KernelStackPages * 4096;
constexpr uint64 UserStackPages = 4;
constexpr uint64 UserStackSize = UserStackPages * 4096;

struct ThreadBlockEvent {
    enum {
        TYPE_NONE,
        TYPE_WAIT,
        TYPE_QUEUE_LOCK,
        TYPE_NODE_READ,
        TYPE_NODE_WRITE,
    } type;

    union {
        struct {
            uint64 remainingMillis;
        } wait;
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

    bool sticky;
    bool killPending;

    IDT::Registers registers;
};

constexpr uint64 SignalNone = 0;
constexpr uint64 SignalDiv0 = 0x1;
constexpr uint64 SignalInvOp = 0x2;
constexpr uint64 SignalGpFault = 0x4;
constexpr uint64 SignalPageFault = 0x8;
constexpr uint64 SignalAbort = 0x10;

