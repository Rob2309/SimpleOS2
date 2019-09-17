#pragma once

#include "types.h"
#include "interrupts/IDT.h"
#include "user/User.h"

struct ThreadInfo;

namespace Scheduler {

    void MakeMeIdleThread();

    /**
     * Create a new Kernel Thread that is not associated with any Process
     * @param rip the Entry point of the Thread
     * @returns the TID of the created Thread
     **/
    uint64 CreateKernelThread(int64 (*func)(uint64, uint64), uint64 arg1 = 0, uint64 arg2 = 0);

    void ThreadDetach(uint64 tid);

    bool ThreadGetChildInfo(uint64 tid, int64& exitCode);

    /**
     * Suspends the currently active Thread and starts the next one.
     * @param regs [in]  The register state of the currently running Thread; 
     *             [out] The register state of the next Thread
     * Should only ever be called by the Timer interrupt handler
     **/
    void Tick(IDT::Registers* regs);

    /**
     * Gives the processor to another thread.
     * Can be called from a KernelThread
     **/
    void ThreadYield();
    /**
     * Suspends the active thread for *at least* [ms] milliseconds.
     * Can be called from a KernelThread
     **/
    void ThreadSleep(uint64 ms);

    /**
     * Destroys the current Thread.
     * Can be called from a KernelThread
     **/
    void ThreadExit(uint64 code);

    /**
     * Get the ThreadID of the currently active Thread
     **/
    uint64 ThreadGetTID();

    /**
     * Get the UID of the process owner
     **/
    uint64 ThreadGetUID();
    /**
     * Get the GID of the process owner
     **/
    uint64 ThreadGetGID();
    /**
     * Get the username of the process owner
     **/
    const char* ThreadGetUserName();

    int64 ThreadAddFileDescriptor(uint64 sysDescriptor);
    /**
     * Copies oldPDesc to newPDesc.
     * Attention:
     *      1. if oldPDesc == newPDesc, this function does nothing and returns OK
     *      2. if oldPDesc is invalid, this function does nothing and returns ErrorInvalidFD
     *      3. if oldPDesc and newPDesc refer to the same File descriptor, this function does nothing and returns OK
     *      4. if newPDesc is already open, this function closes newPDesc first
     **/
    int64 ThreadReplaceFileDescriptor(int64 oldPDesc, int64 newPDesc);
    int64 ThreadCloseFileDescriptor(int64 desc);
    int64 ThreadGetSystemFileDescriptor(int64 pDesc, uint64& sysDesc);

    void ThreadSetFS(uint64 val);
    void ThreadSetGS(uint64 val);

    /**
     * Replaces the Memory space of the current Process with the given pml4Entry
     */
    void ThreadExec(uint64 pml4Entry, IDT::Registers* regs);

    void ThreadKillFromInterrupt(IDT::Registers* regs, const char* reason);
    void ThreadKill(const char* reason);

    void ThreadSetSticky();
    void ThreadUnsetSticky();

    void ThreadDisableInterrupts();
    void ThreadEnableInterrupts();

    void ThreadSetUnkillable(bool unkillable);

    /**
     * If this function is called with a non zero value, the thread will not be killed when a page fault occurs.
     * Instead the thread will just jump to the address given by the rip parameter.
     * Used in kmemcpy_usersafe and kmemset_usersafe.
     **/
    extern "C" void ThreadSetPageFaultRip(uint64 rip);
    /**
     * Called by the page fault interrupt handler to let a thread know it caused a page fault.
     **/
    void ThreadSetupPageFaultHandler(IDT::Registers* regs);

    /**
     * Moves the calling thread to the given logical CPU
     **/
    void ThreadMoveToCPU(uint64 logicalCoreID);

    ThreadInfo* GetCurrentThreadInfo();

}