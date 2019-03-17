#pragma once

#include "types.h"

namespace Syscall
{
    constexpr uint8 InterruptNumber = 0x80;

    constexpr uint64 FunctionGetPID = 0;
    constexpr uint64 FunctionPrint = 1;
    constexpr uint64 FunctionWait = 2;
    constexpr uint64 FunctionExit = 3;
    constexpr uint64 FunctionFork = 4;

    constexpr uint64 FunctionOpen = 10;
    constexpr uint64 FunctionClose = 11;
    constexpr uint64 FunctionRead = 12;
    constexpr uint64 FunctionWrite = 13;

    constexpr uint64 OpenModeRead = 0x1;
    constexpr uint64 OpenModeWrite = 0x2;
    inline uint64 Open(const char* path, uint64 mode)
    {
        uint64 file;
        __asm__ __volatile__ (
            "int $0x80"
            : "=a" (file)
            : "a" (FunctionOpen), "D" (path), "S" (mode)
        );
        return file;
    }
    inline void Close(uint64 file)
    {
        __asm__ __volatile__ (
            "int $0x80"
            : : "a" (FunctionClose), "D" (file)
        );
    }
    inline uint64 Read(uint64 file, void* buffer, uint64 bufferSize)
    {
        uint64 length;
        __asm__ __volatile__ (
            "movq %4, %%r10;"
            "int $0x80"
            : "=a" (length)
            : "a" (FunctionRead), "D"(file), "S"(buffer), "r"(bufferSize)
        );
        return length;
    }
    inline void Write(uint64 file, void* buffer, uint64 bufferSize)
    {
        __asm__ __volatile__ (
            "movq %3, %%r10;"
            "int $0x80"
            : : "a" (FunctionWrite), "D"(file), "S"(buffer), "r"(bufferSize)
        );
    }

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

    inline uint64 Fork()
    {
        uint64 ret;
        __asm__ __volatile__ (
            "int $0x80"
            : "=a"(ret) : "a"(FunctionFork)
        );
        return ret;
    }
}