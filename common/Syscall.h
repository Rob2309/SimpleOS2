#pragma once

#include "types.h"

namespace Syscall
{
    constexpr uint8 InterruptNumber = 0x80;

    constexpr uint64 FunctionGetPID = 0;
    constexpr uint64 FunctionPrint = 1;
    constexpr uint64 FunctionWait = 2;
    constexpr uint64 FunctionExit = 3;

    inline void Print(const char* msg)
    {
        __asm__ __volatile__ (
            "int $0x80"
            : : "a"(FunctionPrint), "D"(msg)
        );
    }

    inline void Wait(uint64 ms) 
    {
        __asm__ __volatile__ (
            "int $0x80"
            : : "a"(FunctionWait), "D"(ms)
        );
    }

    inline uint64 GetPID()
    {
        uint64 pid;
        __asm__ __volatile__ (
            "int $0x80"
            : "=a"(pid) 
            : "a"(FunctionGetPID)
        );
        return pid;
    }

    inline void Exit(uint64 code)
    {
        __asm__ __volatile__ (
            "int $0x80"
            : : "a"(FunctionExit), "D"(code)
        );
    }
}