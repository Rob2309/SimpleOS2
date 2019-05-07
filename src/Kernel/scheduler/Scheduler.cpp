#include "Scheduler.h"

#include "Process.h"
#include "stl/list.h"
#include "memory/memutil.h"
#include "interrupts/IDT.h"
#include "arch/GDT.h"
#include "memory/MemoryManager.h"
#include "arch/MSR.h"
#include "arch/APIC.h"
#include "terminal/conio.h"
#include "arch/CPU.h"
#include "fs/VFS.h"

namespace Scheduler {

    extern "C" void IdleThread();

    static std::nlist<ThreadInfo> g_ThreadList;
    static std::nlist<ThreadInfo> g_DeadThreads;

    static uint64 g_PIDCounter = 1;
    static uint64 g_TIDCounter = 1;

    static ThreadInfo* FindNextThread();

    extern "C" void ReturnToThread(IDT::Registers* regs);
    extern "C" void ContextSwitchAndReturn(IDT::Registers* from, IDT::Registers* to);

    static struct {
        uint64 currentThreadKernelStack;

        ThreadInfo* idleThread;
        ThreadInfo* currentThread;
    } g_CPUData;

    static void TimerEvent(IDT::Registers* regs)
    {
        Scheduler::Tick(regs);
    }

    static ThreadInfo* CreateThreadStruct()
    {
        IDT::DisableInterrupts();
        if(!g_DeadThreads.empty()) {
            ThreadInfo* res = &g_DeadThreads.back();
            g_DeadThreads.pop_back();
            IDT::EnableInterrupts();
            return res;
        }

        IDT::EnableInterrupts();
        
        ThreadInfo* n = new ThreadInfo();
        n->kernelStack = (uint64)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages(KernelStackPages)) + KernelStackSize;
        return n;
    }

    uint64 CreateProcess(uint64 pml4Entry, IDT::Registers* regs)
    {
        ProcessInfo* pInfo = new ProcessInfo();
        pInfo->pid = g_PIDCounter++;
        pInfo->pml4Entry = pml4Entry;
        pInfo->fileDescIDCounter = 1;

        ThreadInfo* tInfo = CreateThreadStruct();
        tInfo->tid = g_TIDCounter++;
        tInfo->process = pInfo;
        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        tInfo->userGSBase = 0;
        tInfo->registers = *regs;

        pInfo->threads.push_back(tInfo);
        
        IDT::DisableInterrupts();
        g_ThreadList.push_back(tInfo);
        uint64 ret = pInfo->pid;
        IDT::EnableInterrupts();

        return ret;
    }

    uint64 CreateKernelThread(uint64 rip)
    {
        ThreadInfo* tInfo = CreateThreadStruct();

        IDT::Registers regs;
        memset(&regs, 0, sizeof(IDT::Registers));
        regs.cs = GDT::KernelCode;
        regs.ds = GDT::KernelData;
        regs.ss = GDT::KernelData;
        regs.rflags = CPU::FLAGS_IF;
        regs.rip = rip;
        regs.userrsp = tInfo->kernelStack;

        tInfo->tid = g_TIDCounter++;
        tInfo->process = nullptr;
        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        tInfo->userGSBase = 0;
        tInfo->registers = regs;
        
        IDT::DisableInterrupts();
        g_ThreadList.push_back(tInfo);
        uint64 ret = tInfo->tid;
        IDT::EnableInterrupts();

        return ret;
    }

    uint64 CloneProcess(IDT::Registers* regs)
    {
        ProcessInfo* oldPInfo = g_CPUData.currentThread->process;

        uint64 pml4Entry = MemoryManager::ForkProcessMap();

        ProcessInfo* pInfo = new ProcessInfo();
        pInfo->pid = g_PIDCounter++;
        pInfo->pml4Entry = pml4Entry;

        oldPInfo->fileDescLock.SpinLock();
        pInfo->fileDescIDCounter = oldPInfo->fileDescIDCounter;
        for(auto oldDesc : oldPInfo->fileDescriptors) {
            FileDescriptor* newDesc = new FileDescriptor();
            newDesc->id = oldDesc->id;
            newDesc->nodeID = oldDesc->nodeID;
            newDesc->readable = oldDesc->readable;
            newDesc->writable = oldDesc->writable;
            
            VFS::Node* n = VFS::AcquireNode(newDesc->id);
            n->refCount++;
            VFS::ReleaseNode(n);
            pInfo->fileDescriptors.push_back(newDesc);
        }
        oldPInfo->fileDescLock.Unlock();

        ThreadInfo* tInfo = CreateThreadStruct();
        tInfo->tid = g_TIDCounter++;
        tInfo->process = pInfo;
        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        tInfo->userGSBase = 0;
        tInfo->registers = *regs;

        pInfo->threads.push_back(tInfo);
        
        IDT::DisableInterrupts();
        g_ThreadList.push_back(tInfo);
        uint64 ret = pInfo->pid;
        IDT::EnableInterrupts();

        return ret;
    }

    static void UpdateEvents() {
        for(auto p : g_ThreadList) {
            switch(p->blockEvent.type) {
            case ThreadBlockEvent::TYPE_WAIT:
                if(p->blockEvent.wait.remainingMillis <= 10) {
                    p->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
                } else {
                    p->blockEvent.wait.remainingMillis -= 10;
                }
                break;
            case ThreadBlockEvent::TYPE_MUTEX:
                if(p->blockEvent.mutex.lock->TryLock()) {
                    p->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
                }
            }
        }
    }

    static ThreadInfo* FindNextThread() {
        for(auto a = g_ThreadList.begin(); a != g_ThreadList.end(); ++a) {
            ThreadInfo* p = *a;
            if(p->blockEvent.type == ThreadBlockEvent::TYPE_NONE) {
                g_ThreadList.erase(a);
                g_ThreadList.push_back(p);
                return p;
            }
        }

        return g_CPUData.idleThread;
    }

    static void SaveThreadInfo(IDT::Registers* regs)
    {
        g_CPUData.currentThread->registers = *regs;     // save all registers in process info
    }

    static void SetContext(ThreadInfo* thread, IDT::Registers* regs)
    {
        g_CPUData.currentThread = thread;
        // Load paging structures for the process
        if(thread->process != nullptr)
            MemoryManager::SwitchProcessMap(thread->process->pml4Entry);
        // Load registers from saved state
        *regs = g_CPUData.currentThread->registers;
        g_CPUData.currentThreadKernelStack = g_CPUData.currentThread->kernelStack;
        bool inKernelMode = ((g_CPUData.currentThread->registers.cs & 0b11) == 0);
        MSR::Write(MSR::RegKernelGSBase, inKernelMode ? g_CPUData.currentThread->userGSBase : (uint64)&g_CPUData);
        MSR::Write(MSR::RegGSBase, inKernelMode ? (uint64)&g_CPUData : g_CPUData.currentThread->userGSBase);
    }

    void Tick(IDT::Registers* regs)
    {
        SaveThreadInfo(regs);

        // Find next process to execute
        UpdateEvents();
        ThreadInfo* nextProcess = FindNextThread();

        SetContext(nextProcess, regs);
    }

    void Start()
    {
        APIC::SetTimerEvent(TimerEvent);

        // Init idle process
        ThreadInfo* p = new ThreadInfo();
        memset(p, 0, sizeof(ThreadInfo));
        p->tid = 0;
        p->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        p->process = nullptr;
        p->registers.rip = (uint64)&IdleThread;
        p->registers.cs = GDT::KernelCode;
        p->registers.ds = GDT::KernelData;
        p->registers.ss = GDT::KernelData;
        p->registers.rflags = CPU::FLAGS_IF;

        g_CPUData.idleThread = p;
        g_CPUData.currentThread = p;

        __asm__ __volatile__ (
            "pushq $0x10;"      // kernel data selector
            "pushq %0;"         // rsp
            "pushq %1;"         // rflags
            "pushq $0x08;"      // kernel code selector
            "pushq %2;"         // rip
            "movq $0x10, %%rax;"// load kernel data selectors
            "mov %%rax, %%ds;"
            "mov %%rax, %%es;"
            "mov $0, %%rax;"    // This should not be required, but if left out, qemu always treats gs.base as zero, even when msr GS_BASE is set to non-zero values
            "mov %%rax, %%fs;"
            "mov %%rax, %%gs;"
            "iretq"             // "return" to idle process
            : : "r"(p->kernelStack), "r"(p->registers.rflags), "r"(p->registers.rip)
        );
    }

    static void Yield()
    {
        IDT::Registers* myRegs = &g_CPUData.currentThread->registers;

        ThreadInfo* nextThread = FindNextThread();

        IDT::Registers nextRegs;
        SetContext(nextThread, &nextRegs);
        ContextSwitchAndReturn(myRegs, &nextRegs);
    }

    void ThreadWait(uint64 ms)
    {
        IDT::DisableInterrupts();

        g_CPUData.currentThread->blockEvent.type = ThreadBlockEvent::TYPE_WAIT;
        g_CPUData.currentThread->blockEvent.wait.remainingMillis = ms;

        Yield();
        IDT::EnableInterrupts();
    }
    void ThreadWaitForLock(void* lock)
    {
        IDT::DisableInterrupts();
        g_CPUData.currentThread->blockEvent.type = ThreadBlockEvent::TYPE_MUTEX;
        g_CPUData.currentThread->blockEvent.mutex.lock = (Mutex*)lock;
        
        Yield();
        IDT::EnableInterrupts();
    }

    void ThreadWaitForNodeRead(uint64 node)
    {
        IDT::DisableInterrupts();
        g_CPUData.currentThread->blockEvent.type = ThreadBlockEvent::TYPE_NODE_READ;
        g_CPUData.currentThread->blockEvent.node.nodeID = node;
        
        Yield();
        IDT::EnableInterrupts();
    }
    void ThreadWaitForNodeWrite(uint64 node)
    {
        IDT::DisableInterrupts();
        g_CPUData.currentThread->blockEvent.type = ThreadBlockEvent::TYPE_NODE_WRITE;
        g_CPUData.currentThread->blockEvent.node.nodeID = node;
        
        Yield();
        IDT::EnableInterrupts();
    }

    static void FreeProcess(ProcessInfo* pInfo)
    {
        MemoryManager::FreeProcessMap(pInfo->pml4Entry);
        for(auto a = pInfo->fileDescriptors.begin(); a != pInfo->fileDescriptors.end();) {
            VFS::CloseNode(a->nodeID);
            delete *(a++);
        }
    }

    void ThreadExit(uint64 code)
    {
        ThreadInfo* tInfo = g_CPUData.currentThread;
        ProcessInfo* pInfo = tInfo->process;

        if(pInfo != nullptr) {
            IDT::DisableInterrupts();
            pInfo->threads.erase(tInfo);
            if(pInfo->threads.empty()) {
                tInfo->process = nullptr;
                IDT::EnableInterrupts();

                FreeProcess(pInfo);
            }
        }

        IDT::DisableInterrupts();
        g_ThreadList.erase(tInfo);
        g_DeadThreads.push_back(tInfo);
        
        IDT::Registers regs;
        ThreadInfo* next = FindNextThread();
        SetContext(next, &regs);
        
        ReturnToThread(&regs);
    }

    uint64 ThreadGetTID()
    {
        return g_CPUData.currentThread->tid;
    }
    uint64 ThreadGetPID()
    {  
        if(g_CPUData.currentThread->process != nullptr)
            return g_CPUData.currentThread->process->pid;
        else
            return 0;    
    }

    uint64 ThreadCreateThread(uint64 entry, uint64 stack)
    {
        ThreadInfo* tInfo = CreateThreadStruct();
        tInfo->tid = g_TIDCounter++;
        tInfo->process = g_CPUData.currentThread->process;
        tInfo->userGSBase = 0;
        tInfo->registers.cs = GDT::UserCode;
        tInfo->registers.ds = GDT::UserData;
        tInfo->registers.ss = GDT::UserData;
        tInfo->registers.rflags = CPU::FLAGS_IF;
        tInfo->registers.rip = entry;
        tInfo->registers.userrsp = stack;

        if(tInfo->process != nullptr) {
            IDT::DisableInterrupts();
            tInfo->process->threads.push_back(tInfo);
        }

        IDT::DisableInterrupts();
        g_ThreadList.push_back(tInfo);
        uint64 res = tInfo->tid;
        IDT::EnableInterrupts();

        return res;
    }

    void NotifyNodeRead(uint64 nodeID)
    {
        IDT::DisableInterrupts();
        for(auto t : g_ThreadList) {
            if(t->blockEvent.type == ThreadBlockEvent::TYPE_NODE_READ && t->blockEvent.node.nodeID == nodeID)
                t->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        }
        IDT::EnableInterrupts();
    }
    void NotifyNodeWrite(uint64 nodeID)
    {
        IDT::DisableInterrupts();
        for(auto t : g_ThreadList) {
            if(t->blockEvent.type == ThreadBlockEvent::TYPE_NODE_WRITE && t->blockEvent.node.nodeID == nodeID)
                t->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        }
        IDT::EnableInterrupts();
    }

    uint64 ProcessAddFileDescriptor(uint64 nodeID, bool read, bool write)
    {
        ProcessInfo* pInfo = g_CPUData.currentThread->process;
        pInfo->fileDescLock.SpinLock();

        FileDescriptor* desc = new FileDescriptor();
        desc->nodeID = nodeID;
        desc->readable = read;
        desc->writable = write;
        desc->id = pInfo->fileDescIDCounter++;
        pInfo->fileDescriptors.push_back(desc);

        uint64 res = desc->id;
        pInfo->fileDescLock.Unlock();
        return res;
    }
    void ProcessRemoveFileDescriptor(uint64 id)
    {
        ProcessInfo* pInfo = g_CPUData.currentThread->process;
        pInfo->fileDescLock.SpinLock();

        for(auto a = pInfo->fileDescriptors.begin(); a != pInfo->fileDescriptors.end(); ++a) {
            if(a->id == id) {
                pInfo->fileDescriptors.erase(a);
                delete *a;
                pInfo->fileDescLock.Unlock();
                return;
            }
        }

        pInfo->fileDescLock.Unlock();
    }
    uint64 ProcessGetFileDescriptorNode(uint64 id)
    {
        ProcessInfo* pInfo = g_CPUData.currentThread->process;
        pInfo->fileDescLock.SpinLock();

        for(auto a = pInfo->fileDescriptors.begin(); a != pInfo->fileDescriptors.end(); ++a) {
            if(a->id == id) {
                uint64 res = a->nodeID;
                pInfo->fileDescLock.Unlock();
                return res;
            }
        }

        pInfo->fileDescLock.Unlock();
        return 0;
    }

    void ProcessExec(uint64 pml4Entry, IDT::Registers* regs)
    {
        IDT::DisableInterrupts();
        uint64 oldPML4Entry = g_CPUData.currentThread->process->pml4Entry;
        g_CPUData.currentThread->process->pml4Entry = pml4Entry;
        IDT::EnableInterrupts();

        MemoryManager::FreeProcessMap(oldPML4Entry);

        IDT::DisableInterrupts();

        SaveThreadInfo(regs);
        SetContext(g_CPUData.currentThread, regs);
        ReturnToThread(regs);
    }

}