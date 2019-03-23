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

    inline uint64 Open(const char* path)
    {
        uint64 fileDesc;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(fileDesc) 
            : "a"(FunctionOpen), "S"(path)
            : "r10", "r11", "rcx"
        );
        return fileDesc;
    }
    inline void Close(uint64 desc)
    {
        __asm__ __volatile__ (
            "syscall"
            : : "a"(FunctionClose), "S"(desc)
            : "r10", "r11", "rcx"
        );
    }
    inline uint64 Read(uint64 desc, uint64 pos, void* buffer, uint64 bufferSize) 
    {
        uint64 ret;
        __asm__ __volatile__ (
            "movq %4, %%r10;"
            "movq %5, %%r11;"
            "int $0x80"
            : "=a"(ret) : "a"(FunctionRead), "D"(buffer), "S"(desc), "r"(bufferSize), "r"(pos)
        );
        return ret;
    }
    inline void Write(uint64 desc, uint64 pos, void* buffer, uint64 bufferSize)
    {
        __asm__ __volatile__ (
            "movq %3, %%r10;"
            "movq %4, %%r11;"
            "int $0x80"
            : : "a"(FunctionWrite), "D"(desc), "S"(buffer), "r"(bufferSize), "r"(pos)
        );
    }

    inline void Print(const char* msg)
    {
        __asm__ __volatile__ (
            "syscall"
            : : "a"(FunctionPrint), "S"(msg)
            : "r10", "r11", "rcx"
        );
    }

    inline void Wait(uint64 ms) 
    {
        __asm__ __volatile__ (
            "syscall"
            : : "a"(FunctionWait), "S"(ms)
            : "r10", "r11", "rcx"
        );
    }

    inline uint64 GetPID()
    {
        uint64 pid;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(pid) 
            : "a"(FunctionGetPID)
            : "r10", "r11", "rcx"
        );
        return pid;
    }

    inline void Exit(uint64 code)
    {
        __asm__ __volatile__ (
            "syscall"
            : : "a"(FunctionExit), "S"(code)
            : "r10", "r11", "rcx"
        );
    }

    inline uint64 Fork()
    {
        uint64 ret;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(ret) 
            : "a"(FunctionFork)
            : "r10", "r11", "rcx"
        );
        return ret;
    }
}