#pragma once

#include "types.h"
#include "interrupts/IDT.h"
#include "user/User.h"

struct ThreadInfo;

namespace Scheduler {

    /**
     * Used to create the very first thread of the system.
     **/
    uint64 CreateInitThread(void (*func)());

    /**
     * Create a new userspace Thread and Process with the given Memory space
     * @param pml4Entry the Paging Structure to user for the process
     * @param regs the initial register values for the created Thread
     * @returns the TID of the created Thread
     **/
    uint64 CreateUserThread(uint64 pml4Entry, IDT::Registers* regs, User* owner);
    /**
     * Create a new Kernel Thread that is not associated with any Process
     * @param rip the Entry point of the Thread
     * @returns the TID of the created Thread
     **/
    uint64 CreateKernelThread(uint64 rip, uint64 arg1 = 0, uint64 arg2 = 0);
    /**
     * Clones the currently active Process and its User Memory Space. All Open FileDescriptors will be cloned onto the new Process.
     * The new Process will have a single Thread.
     * @param regs the initial register values for the created Thread
     * @returns the TID of the newly created Thread
     **/
    uint64 CloneProcess(IDT::Registers* regs);

    /**
     * Performs basic initialization of the Scheduler.
     * Has to be called before any Core calls the Start() function
     */
    void Init(uint64 numCores);
    /**
     * This function is called by the Kernel to start normal Scheduling procedures. It never returns.
     **/
    void Start();

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
    void ThreadWait(uint64 ms);

    /**
     * Destroys the current Thread.
     * Can be called from a KernelThread
     **/
    void ThreadExit(uint64 code);
    /**
     * Creates a new Thread in the currently active process.
     * Can *not* be called from a KernelThread, use CreateKernelThread instead
     * @returns the TID of the newly created Thread
     **/
    uint64 ThreadCreateThread(uint64 entry, uint64 stack, uint64 arg);
    /**
     * Get the ThreadID of the currently active Thread
     **/
    uint64 ThreadGetTID();
    /**
     * Get the ProcessID of the currently active Process.
     * Returns 0 if the current Thread does not belong to a process (KernelThread)
     **/
    uint64 ThreadGetPID();
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

    int64 ProcessAddFileDescriptor(uint64 sysDescriptor);
    int64 ProcessReplaceFileDescriptor(int64 oldPDesc, int64 newPDesc);
    int64 ProcessReplaceFileDescriptorValue(int64 oldPDesc, uint64 newSysDesc);
    int64 ProcessCloseFileDescriptor(int64 desc);
    int64 ProcessGetSystemFileDescriptor(int64 pDesc, uint64& sysDesc);

    void ThreadSetFS(uint64 val);
    void ThreadSetGS(uint64 val);

    /**
     * Replaces the Memory space of the current Process with the given pml4Entry
     */
    void ProcessExec(uint64 pml4Entry, IDT::Registers* regs);

    void ThreadKillProcessFromInterrupt(IDT::Registers* regs, const char* reason);
    void ThreadKillProcess(const char* reason);

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