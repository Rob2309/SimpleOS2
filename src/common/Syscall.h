#pragma once

#include "types.h"

namespace Syscall
{
    constexpr uint64 FunctionGetPID = 0;
    constexpr uint64 FunctionPrint = 1;
    constexpr uint64 FunctionWait = 2;
    constexpr uint64 FunctionExit = 3;
    constexpr uint64 FunctionFork = 4;
    constexpr uint64 FunctionExec = 5;
    constexpr uint64 FunctionCreateThread = 6;
    constexpr uint64 FunctionWaitForLock = 7;

    constexpr uint64 FunctionOpen = 10;
    constexpr uint64 FunctionClose = 11;
    constexpr uint64 FunctionRead = 12;
    constexpr uint64 FunctionWrite = 13;
    constexpr uint64 FunctionPipe = 14;

    inline uint64 Open(const char* path)
    {
        uint64 fileDesc;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(fileDesc) 
            : "D"(FunctionOpen), "S"(path)
            : "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
        return fileDesc;
    }
    inline void Close(uint64 desc)
    {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionClose), "S"(desc)
            : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
    }
    inline uint64 Read(uint64 desc, uint64 pos, void* buffer, uint64 bufferSize) 
    {
        uint64 ret;
        register uint64 r8 __asm__("r8") = (uint64)buffer;
        register uint64 r9 __asm__("r9") = bufferSize;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(ret)
            : "D"(FunctionRead), "S"(desc), "d"(pos), "r"(r8), "r"(r9)
            : "rcx", "r10", "r11"
        );
        return ret;
    }
    inline uint64 Write(uint64 desc, uint64 pos, void* buffer, uint64 bufferSize)
    {
        uint64 ret;
        register uint64 r8 __asm__("r8") = (uint64)buffer;
        register uint64 r9 __asm__("r9") = bufferSize;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(ret)
            : "D"(FunctionWrite), "S"(desc), "d"(pos), "r"(r8), "r"(r9)
            : "rcx", "r10", "r11"
        );
        return ret;
    }

    inline void Pipe(uint64* descs) {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionPipe), "S"(descs)
            : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
    }

    inline void Print(const char* msg)
    {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionPrint), "S"(msg)
            : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
    }

    inline void Wait(uint64 ms) 
    {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionWait), "S"(ms)
            : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
    }

    inline void WaitForLock(void* lock)
    {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionWaitForLock), "S"(lock)
            : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
    }

    inline uint64 GetPID()
    {
        uint64 pid;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(pid) 
            : "D"(FunctionGetPID)
            : "rcx", "rdx", "rsi", "r8", "r9", "r10", "r11"
        );
        return pid;
    }

    inline void Exit(uint64 code)
    {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionExit), "S"(code)
            : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
    }

    inline uint64 Fork()
    {
        uint64 ret;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(ret) 
            : "D"(FunctionFork)
            : "rcx", "rdx", "rsi", "r8", "r9", "r10", "r11"
        );
        return ret;
    }

    inline void Exec(const char* file)
    {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionExec), "S"(file)
            : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
    }

    inline void CreateThread(uint64 entry, uint64 stack)
    {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionCreateThread), "S"(entry), "d"(stack)
            : "rax", "rcx", "r8", "r9", "r10", "r11"
        );
    }
}