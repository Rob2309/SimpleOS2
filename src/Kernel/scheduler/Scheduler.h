#pragma once

#include "types.h"
#include "interrupts/IDT.h"

#include "Thread.h"

namespace Scheduler {

    /**
     * Initializes the Idle thread of the calling CPU core.
     * After this call, every function that only works while in a thread context will work normally
     **/
    void MakeMeIdleThread();

    /**
     * Creates a KernelThread that will be used as the init thread.
     * Every orphaned thread will be attached to this thread.
     **/
    void CreateInitKernelThread(int64 (*func)(uint64, uint64));

    /**
     * Create a new Kernel Thread
     * @param func      the Entry point of the Thread
     * @param arg1      optional argument 1
     * @param arg2      optional argument 2
     * @returns         the TID of the created Thread
     **/
    int64 CreateKernelThread(int64 (*func)(uint64, uint64), uint64 arg1 = 0, uint64 arg2 = 0);

    /**
     * Clones the current Thread
     * @param subthread     If true, the cloned thread will be a subthread of the current thread
     * @param shareMemSpace If true, the cloned thread will share the current thread's memory space, else the memory space will be cloned
     * @param shareFDs      If true, the cloned thread will share the current thread's file descriptors, else the file descriptors will be cloned
     * @param regs          The initial register state of the new thread
     * @returns             The new threads tid
     **/
    int64 CloneThread(bool subthread, bool shareMemSpace, bool shareFDs, IDT::Registers* regs);

    /**
     * Blocks the thread with the given event until
     *      1. The event finishes
     *      2. The blocking state gets interrupted
     * @returns             Zero if the given event finished successfully, non-zero otherwise
     **/
    int64 ThreadBlock(ThreadState::Type type, uint64 arg);

    /**
     * Detaches a thread from the current thread.
     * After detaching a thread, it cannot be joined anymore.
     * @param tid           The tid of a child thread to detach.
     * @returns             OK if successful, error otherwise
     * @note In a multithreaded program, any thread can detach a child thread, no matter which thread created it.
     * @note Subthreads of a program cannot be detached.
     **/
    int64 ThreadDetach(int64 tid);

    /**
     * Joins a child thread.
     * @param tid           The tid of a child thread to join
     * @returns             OK if successful, error otherwise
     * @note In a multithreaded program, any thread can join a child thread, no matter which thread created it.
     **/
    int64 ThreadJoin(int64 tid, int64& exitCode);

    /**
     * Joins a child thread without blocking.
     * @param tid           The tid of a child thread to join
     * @returns             OK if successful, error otherwise
     * @note In a multithreaded program, any thread can join a child thread, no matter which thread created it.
     **/
    int64 ThreadTryJoin(int64 tid, int64& exitCode);

    /**
     * Requests a thread to terminate
     * @param tid           The tid of a thread to terminate. Has to be a thread owned by the same user (if current user is not root).
     * @returns             OK if successful, error otherwise
     * @note A thread can kill itself
     **/
    int64 ThreadKill(int64 tid);

    /**
     * Suspends the currently active Thread and starts the next one.
     * @param regs [in]  The register state of the currently running Thread; 
     *             [out] The register state of the next Thread
     * Should only ever be called by the Timer interrupt handler
     **/
    void Tick(IDT::Registers* regs);

    /**
     * Gives the processor to another thread.
     **/
    void ThreadYield();
    /**
     * Suspends the active thread for *at least* [ms] milliseconds.
     **/
    int64 ThreadSleep(uint64 ms);

    /**
     * Exits the current Thread.
     **/
    void ThreadExit(uint64 code);

    /**
     * Get the ThreadID of the currently active Thread
     **/
    int64 ThreadGetTID();

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

    void ThreadSetSticky();
    void ThreadUnsetSticky();

    void ThreadDisableInterrupts();
    void ThreadEnableInterrupts();

    /**
     * Checks if the current thread was requested to terminate itself, exits if true
     **/
    void ThreadCheckFlags();

    /**
     * If this function is called with a non zero value, the thread will not be killed when a page fault occurs.
     * Instead the thread will just jump to the address given by the rip parameter.
     * Used in kmemcpy_usersafe and kmemset_usersafe.
     **/
    extern "C" void ThreadSetPageFaultRip(uint64 rip);
    /**
     * Called by the fault interrupt handlers to let a thread know it caused a fault.
     **/
    void ThreadSetupFaultHandler(IDT::Registers* regs, const char* msg);

    ThreadInfo* GetCurrentThreadInfo();

}