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

    constexpr uint64 FunctionCreateFile = 50;
    constexpr uint64 FunctionCreateFolder = 51;
    constexpr uint64 FunctionCreateDeviceFile = 52;
    constexpr uint64 FunctionCreatePipe = 53;
    constexpr uint64 FunctionDelete = 54;
    constexpr uint64 FunctionOpen = 55;
    constexpr uint64 FunctionClose = 56;
    constexpr uint64 FunctionList = 57;
    constexpr uint64 FunctionRead = 58;
    constexpr uint64 FunctionWrite = 59;
    constexpr uint64 FunctionMount = 60;
    constexpr uint64 FunctionMountDev = 61;
    constexpr uint64 FunctionChangeOwner = 62;
    constexpr uint64 FunctionChangePermissions = 63;

    constexpr uint64 FunctionAllocPages = 100;
    constexpr uint64 FunctionFreePages = 101;

    constexpr uint64 FunctionMoveToCore = 500;

    inline void MoveThreadToCore(uint64 coreID) {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionMoveToCore), "S"(coreID)
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

    inline int64 Exec(const char* file)
    {
        int64 res;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(res) 
            : "D"(FunctionExec), "S"(file)
            : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
        return res;
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

    inline int64 CreateFile(const char* path) {
        int64 ret;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(ret)
            : "D"(FunctionCreateFile), "S"(path)
            : "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
        return ret;
    }

    inline int64 CreateFolder(const char* path) {
        int64 ret;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(ret)
            : "D"(FunctionCreateFolder), "S"(path)
            : "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
        return ret;
    }

    inline int64 CreateDeviceFile(const char* path, uint64 driverID, uint64 subID) {
        int64 ret;
        register uint64 r8 asm("r8") = subID;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(ret)
            : "D"(FunctionCreateDeviceFile), "S"(path), "d"(driverID), "r"(r8)
            : "rcx", "r9", "r10", "r11"
        );
        return ret;
    }

    inline void CreatePipe(uint64* readDesc, uint64* writeDesc) {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionCreatePipe), "S"(readDesc), "d"(writeDesc)
            : "rcx", "r8", "r9", "r10", "r11"
        );
    }

    inline int64 Delete(const char* path) {
        int64 ret;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(ret)
            : "D"(FunctionDelete), "S"(path)
            : "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
        return ret;
    }

    inline int64 ChangePermissions(const char* path, uint8 ownerPerm, uint8 groupPerm, uint8 otherPerm) {
        int64 ret;
        register uint64 r8 asm("r8") = groupPerm;
        register uint64 r9 asm("r9") = otherPerm;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(ret)
            : "D"(FunctionChangePermissions), "S"(path), "d"(ownerPerm), "r"(r8), "r"(r9)
            : "rcx", "r10", "r11"
        );
        return ret;
    }

    inline int64 Open(const char* path, uint8 perm) {
        int64 ret;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(ret)
            : "D"(FunctionOpen), "S"(path), "d"(perm)
            : "rcx", "r8", "r9", "r10", "r11"
        );
        return ret;
    }

    inline int64 Close(uint64 desc) {
        int64 ret;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(ret)
            : "D"(FunctionClose), "S"(desc)
            : "rcx", "rdx", "r8", "r9", "r10", "r11"
        );
        return ret;
    }

    inline int64 Read(uint64 desc, void* buffer, uint64 bufferSize) {
        int64 res;
        register uint64 r8 asm("r8") = bufferSize;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(res)
            : "D"(FunctionRead), "S"(desc), "d"(buffer), "r"(r8)
            : "rcx", "r9", "r10", "r11"
        );
        return res;
    }

    inline int64 Write(uint64 desc, const void* buffer, uint64 bufferSize) {
        int64 res;
        register uint64 r8 asm("r8") = bufferSize;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(res)
            : "D"(FunctionWrite), "S"(desc), "d"(buffer), "r"(r8)
            : "rcx", "r9", "r10", "r11"
        );
        return res;
    }
    
    inline int64 Mount(const char* mountPoint, const char* fsID) {
        int64 res;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(res)
            : "D"(FunctionMount), "S"(mountPoint), "d"(fsID)
            : "rcx", "r8", "r9", "r10", "r11"
        );
        return res;
    }

    inline int64 Mount(const char* mountPoint, const char* fsID, const char* dev) {
        int64 res;
        register uint64 r8 asm("r8") = (uint64)dev;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(res)
            : "D"(FunctionMountDev), "S"(mountPoint), "d"(fsID), "r"(r8)
            : "rcx", "r9", "r10", "r11"
        );
        return res;
    }

    inline bool AllocPages(void* address, uint64 numPages) {
        bool res;
        __asm__ __volatile__ (
            "syscall"
            : "=a"(res)
            : "D"(FunctionAllocPages), "S"(address), "d"(numPages)
            : "rcx", "r8", "r9", "r10", "r11"
        );
        return res;
    }
    inline void FreePages(void* address, uint64 numPages) {
        __asm__ __volatile__ (
            "syscall"
            : : "D"(FunctionFreePages), "S"(address), "d"(numPages)
            : "rax", "rcx", "r8", "r9", "r10", "r11"
        );
    }
    
}