#pragma once

#include "types.h"
#include "IDT.h"

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

struct ThreadInfo {
    uint64 pid;

    ThreadBlockEvent blockEvent;

    uint64 pml4Entry;
    uint64 kernelStack;

    uint64 kernelGS;
    uint64 userGS;

    IDT::Registers registers;
};