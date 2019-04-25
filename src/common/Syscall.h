#pragma once

#include "types.h"

namespace Syscall
{
    constexpr uint64 FunctionGetPID = 0;
    constexpr uint64 FunctionPrint = 1;
    constexpr uint64 FunctionWait = 2;
    constexpr uint64 FunctionExit = 3;
    constexpr uint64 FunctionFork = 4;
    constexpr uint64 FunctionCreateThread = 5;
    constexpr uint64 FunctionWaitForLock = 6;

    constexpr uint64 FunctionOpen = 10;
    constexpr uint64 FunctionClose = 11;
    constexpr uint64 FunctionRead = 12;
    constexpr uint64 FunctionWrite = 13;
    constexpr uint64 FunctionPipe = 14;

    /**
     * Open the FileSystem Node with the given path
     * @returns the FileDescriptorID of the opened node, 0 on error
     **/
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
    /**
     * Close the given FileDescriptor
     **/
    inline void Close(uint64 desc)
    {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionClose), "S"(desc)
            : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
    }
    /**
     * Read from the given FileDescriptor
     **/
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
    /**
     * Write to the given FileDescriptor
     **/
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

    /**
     * Create a pipe and write the two FileDescriptors associated with it into descs
     **/
    inline void Pipe(uint64* descs) {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionPipe), "S"(descs)
            : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
    }

    /**
     * Print a message onto the screen
     **/
    inline void Print(const char* msg)
    {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionPrint), "S"(msg)
            : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
    }

    /**
     * Wait *at least* [ms] milliseconds
     **/
    inline void Wait(uint64 ms) 
    {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionWait), "S"(ms)
            : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
    }

    /**
     * Wait until the given mutex can be locked by the Scheduler
     **/
    inline void WaitForLock(void* lock)
    {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionWaitForLock), "S"(lock)
            : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
    }

    /**
     * Get the ProcessID of the Process associated with this thread
     **/
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

    /**
     * Exit the current Thread
     **/
    inline void Exit(uint64 code)
    {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionExit), "S"(code)
            : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
    }

    /**
     * Fork the current process
     **/
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

    /**
     * Create a new thread at the given entrypoint
     **/
    inline void CreateThread(uint64 entry, uint64 stack)
    {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionCreateThread), "S"(entry), "d"(stack)
            : "rax", "rcx", "r8", "r9", "r10", "r11"
        );
    }
}