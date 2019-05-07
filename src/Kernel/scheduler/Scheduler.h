#pragma once

#include "types.h"
#include "interrupts/IDT.h"

namespace Scheduler {

    /**
     * Create a new userspace Process with one Thread
     * @param pml4Entry the Paging Structure to user for this process
     * @param regs the initial register values for the created Thread
     **/
    uint64 CreateProcess(uint64 pml4Entry, IDT::Registers* regs);
    /**
     * Create a new Kernel Thread that will be handled like any other Thread by the Scheduler
     * @param rip the Entry point of the Thread
     **/
    uint64 CreateKernelThread(uint64 rip);
    /**
     * Clones the currently active Process and its User Memory Space. All Open FileDescriptors will be cloned onto the new Process.
     * The new Process will have a single Thread.
     * @param regs the initial register values for the created Thread
     **/
    uint64 CloneProcess(IDT::Registers* regs);

    /**
     * This function is called by the Kernel to start normal Scheduling procedures. It never returns.
     **/
    void Start();

    /**
     * Suspends the currently active Thread and starts the next one.
     * @param regs [in]  The register state of the currently running Thread; 
     *             [out] The register state of the next Thread
     **/
    void Tick(IDT::Registers* regs);

    /**
     * Suspends the active thread for *at least* [ms] milliseconds.
     * Can be called from a KernelThread
     **/
    void ThreadWait(uint64 ms);
    /**
     * Suspends the active thread until the mutex given by lock parameter was successfully locked by the Scheduler.
     * Can be called from a KernelThread
     **/
    void ThreadWaitForLock(void* lock);
    /**
     * Suspends the active thread until another thread read at least 1 byte from the given FileSystem Node.
     * Can be called from a KernelThread
     **/
    void ThreadWaitForNodeRead(uint64 node);
    /**
     * Suspends the active thread until another thread has written at least 1 byte to the given FileSystem Node.
     * Can be called from a KernelThread
     **/
    void ThreadWaitForNodeWrite(uint64 node);
    /**
     * Destroys the current Thread.
     * Can be called from a KernelThread
     **/
    void ThreadExit(uint64 code);
    /**
     * Creates a new Thread in the currently active process.
     * Can *not* be called from a KernelThread, use CreateKernelThread instead
     **/
    uint64 ThreadCreateThread(uint64 entry, uint64 stack);
    /**
     * Get the ThreadID of the currently active Thread
     **/
    uint64 ThreadGetTID();
    /**
     * Get the ProcessID of the currently active Process
     **/
    uint64 ThreadGetPID();

    /**
     * Notify the Scheduler that the given node was written to.
     * Should only be called by the FileSystem.
     **/
    void NotifyNodeRead(uint64 nodeID);
    /**
     * Notify the Scheduler that the given node was read from
     * Should only be called by the FileSystem.
     **/
    void NotifyNodeWrite(uint64 nodeID);

    /**
     * Add a FileDescriptor to the currently active Process
     **/
    uint64 ProcessAddFileDescriptor(uint64 nodeID, bool read, bool write);
    /**
     * Remove a FileDescriptor from the currently active Process
     **/
    void ProcessRemoveFileDescriptor(uint64 id);
    /**
     * Get the FileSystem Node associated with the given FileDescriptor
     **/
    uint64 ProcessGetFileDescriptorNode(uint64 id);

    void ProcessExec(uint64 pml4Entry, IDT::Registers* regs);

}