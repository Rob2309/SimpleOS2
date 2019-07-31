#pragma once

#include "types.h"
#include "SyscallFunctions.h"

#define SYSCALL_REGISTER(func, number) uint64 func##_syscall_array_entry[] __attribute__((used)) __attribute__((section(".syscall_array"))) = { (uint64)&func, number }

struct __attribute__((packed)) SyscallState
{
    uint64 userrip, userrsp, userrbp, userflags;
    uint64 userrbx, userr12, userr13, userr14, userr15;
};

#define SYSCALL_DEFINE(number) \
    uint64 syscall_func_##number(uint64, uint64, uint64, uint64, SyscallState*); \
    SYSCALL_REGISTER(syscall_func_##number, number); \
    uint64 syscall_func_##number(uint64 arg1, uint64 arg2, uint64 arg3, uint64 arg4, SyscallState* state)


