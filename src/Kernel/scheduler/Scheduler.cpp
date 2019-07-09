#include "Scheduler.h"

#include "Thread.h"
#include "Process.h"
#include "multicore/SMP.h"
#include "memory/MemoryManager.h"
#include "arch/GDT.h"
#include "arch/CPU.h"
#include "arch/MSR.h"
#include "arch/APIC.h"
#include "fs/VFS.h"
#include "klib/stdio.h"

namespace Scheduler {

    extern "C" void IdleThread();

    static uint64 g_PIDCounter = 1;
    static uint64 g_TIDCounter = 1;

    static ThreadInfo* FindNextThread();

    extern "C" void ReturnToThread(IDT::Registers* regs);
    extern "C" void ContextSwitchAndReturn(IDT::Registers* from, IDT::Registers* to);

    struct CPUData {
        uint64 currentThreadKernelStack;

        ThreadInfo* idleThread;
        ThreadInfo* currentThread;

        ktl::nlist<ThreadInfo> threadList;
        ktl::nlist<ThreadInfo> deadThreads;
    };

    static CPUData* g_CPUData;

    static void TimerEvent(IDT::Registers* regs)
    {
        Scheduler::Tick(regs);
    }

    static StickyLock g_TransferLock;
    static bool g_TransferComplete;
    static ThreadInfo* g_TransferThread;

    static StickyLock g_KillLock;
    static uint64 g_KillCount;
    static ProcessInfo* g_KillProcess;

    static void SetContext(ThreadInfo* thread, IDT::Registers* regs)
    {
        uint64 coreID = SMP::GetLogicalCoreID();

        g_CPUData[coreID].currentThread = thread;
        // Load paging structures for the process
        if(thread->process != nullptr)
            MemoryManager::SwitchProcessMap(thread->process->pml4Entry);
        // Load registers from saved state
        *regs = g_CPUData[coreID].currentThread->registers;
        g_CPUData[coreID].currentThreadKernelStack = g_CPUData[coreID].currentThread->kernelStack;
        bool inKernelMode = ((g_CPUData[coreID].currentThread->registers.cs & 0b11) == 0);
        MSR::Write(MSR::RegKernelGSBase, inKernelMode ? g_CPUData[coreID].currentThread->userGSBase : (uint64)&g_CPUData[coreID]);
        MSR::Write(MSR::RegGSBase, inKernelMode ? (uint64)&g_CPUData[coreID] : g_CPUData[coreID].currentThread->userGSBase);
    }

    static void FreeProcess(ProcessInfo* pInfo)
    {
        MemoryManager::FreeProcessMap(pInfo->pml4Entry);
        delete pInfo;
    }

    static void TransferEvent(IDT::Registers* regs) {
        APIC::SignalEOI();

        uint64 coreID = SMP::GetLogicalCoreID();
        g_CPUData[coreID].threadList.push_back(g_TransferThread);
        g_TransferComplete = true;
    }

    static void SetupKillHandler(ThreadInfo* tInfo, uint64 exitCode);

    static void KillEvent(IDT::Registers* regs) {
        APIC::SignalEOI();

        uint64 coreID = SMP::GetLogicalCoreID();

        ProcessInfo* pInfo = g_KillProcess;

        for(ThreadInfo* t : g_CPUData[coreID].threadList) {
            if(t->process == pInfo) {
                if(t->unkillable) {
                    t->killPending = true;
                    t->killCode = 123;
                } else {
                    SetupKillHandler(t, 123);
                }
            }
        }

        __asm__ __volatile__ (
            "lock incq (%0)"
            : : "r"(&g_KillCount)
        );

        SetContext(g_CPUData[coreID].currentThread, regs);
    }

    static ThreadInfo* CreateThreadStruct()
    {
        uint64 coreID = SMP::GetLogicalCoreID();

        ThreadSetSticky();

        if(!g_CPUData[coreID].deadThreads.empty()) {
            ThreadInfo* res = g_CPUData[coreID].deadThreads.back();
            g_CPUData[coreID].deadThreads.pop_back();
            ThreadUnsetSticky();

            uint64 kernelStack = res->kernelStack;
            kmemset(res, 0, sizeof(ThreadInfo));
            res->kernelStack = kernelStack;

            return res;
        }

        ThreadUnsetSticky();
        
        ThreadInfo* n = new ThreadInfo();
        n->kernelStack = (uint64)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages(KernelStackPages)) + KernelStackSize;
        return n;
    }

    uint64 CreateInitThread(void (*func)()) {
        uint64 coreID = SMP::GetLogicalCoreID();

        ThreadInfo* tInfo = (ThreadInfo*)MemoryManager::PhysToKernelPtr(MemoryManager::EarlyAllocatePages(NUM_PAGES(sizeof(ThreadInfo))));
        kmemset(tInfo, 0, sizeof(ThreadInfo));
        tInfo->kernelStack = (uint64)MemoryManager::PhysToKernelPtr((uint8*)MemoryManager::EarlyAllocatePages(KernelStackPages) + KernelStackSize);

        IDT::Registers regs;
        kmemset(&regs, 0, sizeof(IDT::Registers));
        regs.cs = GDT::KernelCode;
        regs.ds = GDT::KernelData;
        regs.ss = GDT::KernelData;
        regs.rflags = CPU::FLAGS_IF;
        regs.rip = (uint64)func;
        regs.userrsp = tInfo->kernelStack;

        tInfo->tid = g_TIDCounter++;
        tInfo->process = nullptr;
        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        tInfo->userGSBase = 0;
        tInfo->registers = regs;
        tInfo->unkillable = true;
        
        g_CPUData[coreID].threadList.push_back(tInfo);
        uint64 ret = tInfo->tid;

        return ret;
    }

    uint64 CreateUserThread(uint64 pml4Entry, IDT::Registers* regs)
    {
        uint64 coreID = SMP::GetLogicalCoreID();

        ProcessInfo* pInfo = new ProcessInfo();
        pInfo->pid = g_PIDCounter++;
        pInfo->pml4Entry = pml4Entry;

        ThreadInfo* tInfo = CreateThreadStruct();
        tInfo->tid = g_TIDCounter++;
        tInfo->process = pInfo;
        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        tInfo->userGSBase = 0;
        tInfo->registers = *regs;
        tInfo->unkillable = false;
        tInfo->killPending = false;

        pInfo->numThreads = 1;
        
        ThreadSetSticky();
        g_CPUData[coreID].threadList.push_back(tInfo);
        uint64 ret = tInfo->tid;
        ThreadUnsetSticky();

        return ret;
    }

    uint64 CreateKernelThread(uint64 rip)
    {
        uint64 coreID = SMP::GetLogicalCoreID();

        ThreadInfo* tInfo = CreateThreadStruct();

        IDT::Registers regs;
        kmemset(&regs, 0, sizeof(IDT::Registers));
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
        tInfo->unkillable = true;
        tInfo->killPending = false;
        
        ThreadSetSticky();
        g_CPUData[coreID].threadList.push_back(tInfo);
        uint64 ret = tInfo->tid;
        ThreadUnsetSticky();

        return ret;
    }

    uint64 CloneProcess(IDT::Registers* regs)
    {
        uint64 coreID = SMP::GetLogicalCoreID();

        ProcessInfo* oldPInfo = g_CPUData[coreID].currentThread->process;

        uint64 pml4Entry = MemoryManager::ForkProcessMap();

        ProcessInfo* pInfo = new ProcessInfo();
        pInfo->pid = g_PIDCounter++;
        pInfo->pml4Entry = pml4Entry;
        
        oldPInfo->fileDescLock.SpinLock();
        for(ProcessFileDescriptor* fd : oldPInfo->fileDescs) {
            if(fd->desc == 0)
                continue;

            ProcessFileDescriptor* newFD = new ProcessFileDescriptor();
            newFD->desc = fd->desc;
            newFD->id = pInfo->fileDescs.size();
            pInfo->fileDescs.push_back(newFD);
            VFS::AddRef(newFD->desc);
        }
        oldPInfo->fileDescLock.Unlock();

        ThreadInfo* tInfo = CreateThreadStruct();
        tInfo->tid = g_TIDCounter++;
        tInfo->process = pInfo;
        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        tInfo->userGSBase = 0;
        tInfo->registers = *regs;

        pInfo->numThreads = 1;
        
        ThreadSetSticky();
        g_CPUData[coreID].threadList.push_back(tInfo);
        uint64 ret = tInfo->tid;
        ThreadUnsetSticky();

        return ret;
    }

    static void UpdateEvents() {
        uint64 coreID = SMP::GetLogicalCoreID();

        for(auto p : g_CPUData[coreID].threadList) {
            switch(p->blockEvent.type) {
            case ThreadBlockEvent::TYPE_WAIT:
                if(p->blockEvent.wait.remainingMillis <= 10) {
                    p->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
                } else {
                    p->blockEvent.wait.remainingMillis -= 10;
                }
                break;
            }
        }
    }

    static ThreadInfo* FindNextThread() {
        uint64 coreID = SMP::GetLogicalCoreID();

        for(auto a = g_CPUData[coreID].threadList.begin(); a != g_CPUData[coreID].threadList.end(); ++a) {
            ThreadInfo* p = *a;
            if(p->blockEvent.type == ThreadBlockEvent::TYPE_NONE) {
                g_CPUData[coreID].threadList.erase(a);
                g_CPUData[coreID].threadList.push_back(p);
                return p;
            }
        }

        return g_CPUData[coreID].idleThread;
    }

    static void SaveThreadInfo(IDT::Registers* regs)
    {
        uint64 coreID = SMP::GetLogicalCoreID();
        g_CPUData[coreID].currentThread->registers = *regs;     // save all registers in process info
    }

    void Tick(IDT::Registers* regs)
    {
        SaveThreadInfo(regs);

        // Find next process to execute
        UpdateEvents();
        ThreadInfo* nextProcess = FindNextThread();

        SetContext(nextProcess, regs);
    }

    void Init(uint64 numCores) {
        g_CPUData = (CPUData*)MemoryManager::PhysToKernelPtr(MemoryManager::EarlyAllocatePages(NUM_PAGES(numCores * sizeof(CPUData))));
        for(uint64 i = 0; i < numCores; i++)
            new(&g_CPUData[i]) CPUData();

        IDT::SetISR(ISRNumbers::IPIMoveThread, TransferEvent);
        IDT::SetISR(ISRNumbers::IPIKillProcess, KillEvent);
    }

    void Start()
    {
        uint64 coreID = SMP::GetLogicalCoreID();

        IDT::DisableInterrupts();
        APIC::SetTimerEvent(TimerEvent);
        APIC::StartTimer(10);

        // Init idle process
        ThreadInfo* p = (ThreadInfo*)MemoryManager::PhysToKernelPtr(MemoryManager::EarlyAllocatePages(NUM_PAGES(sizeof(ThreadInfo))));
        kmemset(p, 0, sizeof(ThreadInfo));
        p->tid = 0;
        p->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        p->process = nullptr;
        p->registers.rip = (uint64)&IdleThread;
        p->registers.cs = GDT::KernelCode;
        p->registers.ds = GDT::KernelData;
        p->registers.ss = GDT::KernelData;
        p->registers.rflags = CPU::FLAGS_IF;

        g_CPUData[coreID].idleThread = p;
        g_CPUData[coreID].currentThread = p;

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
        uint64 coreID = SMP::GetLogicalCoreID();

        IDT::Registers* myRegs = &g_CPUData[coreID].currentThread->registers;

        // TODO: Find out why this breaks in release mode
        /*ThreadInfo* nextThread = FindNextThread();
        if(nextThread == g_CPUData.currentThread) {
            return;
        } else {
            IDT::Registers nextRegs;
            SetContext(nextThread, &nextRegs);
            ContextSwitchAndReturn(myRegs, &nextRegs);
        }*/
        IDT::Registers nextRegs;
        SetContext(g_CPUData[coreID].idleThread, &nextRegs);
        ContextSwitchAndReturn(myRegs, &nextRegs);
    }

    void ThreadYield()
    {
        ThreadSetSticky();
        Yield();
        ThreadUnsetSticky();
    }

    void ThreadWait(uint64 ms)
    {
        uint64 coreID = SMP::GetLogicalCoreID();

        ThreadSetSticky();

        g_CPUData[coreID].currentThread->blockEvent.type = ThreadBlockEvent::TYPE_WAIT;
        g_CPUData[coreID].currentThread->blockEvent.wait.remainingMillis = ms;
        
        Yield();
        ThreadUnsetSticky();
    }

    void ThreadExit(uint64 code)
    {
        kprintf("%C[%i.%i]%C Exiting with code %i\n", 200, 50, 50, Scheduler::ThreadGetPID(), Scheduler::ThreadGetTID(), 255, 255, 255, code);

        uint64 coreID = SMP::GetLogicalCoreID();

        ThreadInfo* tInfo = g_CPUData[coreID].currentThread;
        ProcessInfo* pInfo = tInfo->process;

        if(pInfo != nullptr) {
            pInfo->mainLock.SpinLock();
            pInfo->numThreads--;
            if(pInfo->numThreads == 0) {
                tInfo->process = nullptr;

                FreeProcess(pInfo);
            } else {
                pInfo->mainLock.Unlock();
            }
        }

        ThreadSetSticky();
        g_CPUData[coreID].threadList.erase(tInfo);
        g_CPUData[coreID].deadThreads.push_back(tInfo);
        
        IDT::Registers regs;
        ThreadInfo* next = FindNextThread();
        SetContext(next, &regs);
        ReturnToThread(&regs);
    }

    uint64 ThreadGetTID()
    {
        uint64 coreID = SMP::GetLogicalCoreID();
        return g_CPUData[coreID].currentThread->tid;
    }
    uint64 ThreadGetPID()
    {  
        uint64 coreID = SMP::GetLogicalCoreID();

        if(g_CPUData[coreID].currentThread->process != nullptr)
            return g_CPUData[coreID].currentThread->process->pid;
        else
            return 0;    
    }

    uint64 ThreadCreateThread(uint64 entry, uint64 stack)
    {
        uint64 coreID = SMP::GetLogicalCoreID();

        ThreadInfo* tInfo = CreateThreadStruct();
        tInfo->tid = g_TIDCounter++;
        tInfo->process = g_CPUData[coreID].currentThread->process;
        tInfo->userGSBase = 0;
        tInfo->registers.cs = GDT::UserCode;
        tInfo->registers.ds = GDT::UserData;
        tInfo->registers.ss = GDT::UserData;
        tInfo->registers.rflags = CPU::FLAGS_IF;
        tInfo->registers.rip = entry;
        tInfo->registers.userrsp = stack;

        if(tInfo->process != nullptr) {
            tInfo->process->mainLock.SpinLock();
            tInfo->process->numThreads++;
            tInfo->process->mainLock.Unlock();
        }

        ThreadSetSticky();
        g_CPUData[coreID].threadList.push_back(tInfo);
        uint64 res = tInfo->tid;
        ThreadUnsetSticky();

        return res;
    }

    uint64 ProcessAddFileDescriptor(uint64 sysDescriptor) {
        uint64 coreID = SMP::GetLogicalCoreID();

        ProcessInfo* pInfo = g_CPUData[coreID].currentThread->process;
        pInfo->fileDescLock.SpinLock();
        for(ProcessFileDescriptor* d : pInfo->fileDescs) {
            if(d->desc == 0) {
                d->desc = sysDescriptor;
                pInfo->fileDescLock.Unlock();
                return d->id;
            }
        }

        ProcessFileDescriptor* newDesc = new ProcessFileDescriptor();
        newDesc->id = pInfo->fileDescs.size();
        newDesc->desc = sysDescriptor;
        pInfo->fileDescs.push_back(newDesc);
        pInfo->fileDescLock.Unlock();

        return newDesc->id;
    };
    void ProcessCloseFileDescriptor(uint64 descID) {
        uint64 coreID = SMP::GetLogicalCoreID();

        ProcessInfo* pInfo = g_CPUData[coreID].currentThread->process;

        pInfo->fileDescLock.SpinLock();
        ProcessFileDescriptor* desc = pInfo->fileDescs[descID];
        uint64 sysDesc = desc->desc;
        desc->desc = 0;
        pInfo->fileDescLock.Unlock();

        VFS::Close(desc->desc);
    }
    uint64 ProcessGetSystemFileDescriptor(uint64 descID) {
        uint64 coreID = SMP::GetLogicalCoreID();

        ProcessInfo* pInfo = g_CPUData[coreID].currentThread->process;

        pInfo->fileDescLock.SpinLock();
        ProcessFileDescriptor* desc = pInfo->fileDescs[descID];
        uint64 res = desc->desc;
        pInfo->fileDescLock.Unlock();
        return res;
    }

    void ProcessExec(uint64 pml4Entry, IDT::Registers* regs)
    {
        uint64 coreID = SMP::GetLogicalCoreID();

        uint64 oldPML4Entry = g_CPUData[coreID].currentThread->process->pml4Entry;
        g_CPUData[coreID].currentThread->process->pml4Entry = pml4Entry;

        MemoryManager::FreeProcessMap(oldPML4Entry);

        IDT::DisableInterrupts();
        SaveThreadInfo(regs);
        SetContext(g_CPUData[coreID].currentThread, regs);
        ReturnToThread(regs);
    }

    static void KillHandler(uint64 exitCode) {
        ThreadExit(exitCode);
    }

    static void SetupKillHandler(ThreadInfo* tInfo, uint64 exitCode) {
        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        tInfo->stickyCount = 1;
        tInfo->unkillable = true;
        tInfo->registers.cs = GDT::KernelCode;
        tInfo->registers.ds = GDT::KernelData;
        tInfo->registers.rdi = exitCode;
        tInfo->registers.rflags = 0;
        tInfo->registers.rip = (uint64)&KillHandler;
        tInfo->registers.ss = GDT::UserData;
        tInfo->registers.userrsp = tInfo->kernelStack;
    }

    void ThreadKillProcessFromInterrupt(IDT::Registers* regs, const char* reason) {
        klog_error("Scheduler", "Killing process %i because Thread %i caused an exception (%s)", ThreadGetPID(), ThreadGetTID(), reason);

        uint64 coreID = SMP::GetLogicalCoreID();

        ThreadInfo* tInfo = g_CPUData[coreID].currentThread;
        ProcessInfo* pInfo = tInfo->process;

        if(pInfo != nullptr) {
            pInfo->mainLock.SpinLock_NoSticky();
            for(ThreadInfo* t : g_CPUData[coreID].threadList) {
                if(t == tInfo) {
                    SetupKillHandler(t, 123);
                } else if(t->process == pInfo) {
                    if(t->unkillable) {
                        t->killPending = true;
                        t->killCode = 123;
                    } else {
                        SetupKillHandler(t, 123);
                    }
                }
            }
            pInfo->mainLock.Unlock_NoSticky();

            g_KillLock.SpinLock_NoSticky();
            g_KillCount = 1;
            g_KillProcess = pInfo;
            APIC::SendIPI(APIC::IPI_TARGET_ALL_BUT_SELF, 0, ISRNumbers::IPIKillProcess);
            while(g_KillCount < SMP::GetCoreCount()) ;
            g_KillLock.Unlock_NoSticky();
        } else {
            // TODO: Kernel panic (KernelThread has caused an exception)
            klog_fatal("PANIC", "KernelThread has caused an exception (TID=%i)", Scheduler::ThreadGetTID());
            while(true) 
                asm("hlt");
        }

        SetContext(tInfo, regs);
    }
    void ThreadKillProcess(const char* reason) {
        klog_error("Scheduler", "Killing process %i because Thread %i requested it (%s)", ThreadGetPID(), ThreadGetTID(), reason);

        uint64 coreID = SMP::GetLogicalCoreID();

        ThreadInfo* tInfo = g_CPUData[coreID].currentThread;
        ProcessInfo* pInfo = tInfo->process;

        if(pInfo != nullptr) {
            pInfo->mainLock.SpinLock();
            ThreadSetSticky();
            for(ThreadInfo* t : g_CPUData[coreID].threadList) {
                if(t->process == pInfo && t != tInfo) {
                    if(t->unkillable) {
                        t->killPending = true;
                        t->killCode = 123;
                    } else {
                        SetupKillHandler(t, 123);
                    }
                }
            }
            ThreadUnsetSticky();
            pInfo->mainLock.Unlock();

            g_KillLock.SpinLock();
            g_KillCount = 1;
            g_KillProcess = pInfo;
            APIC::SendIPI(APIC::IPI_TARGET_ALL_BUT_SELF, 0, ISRNumbers::IPIKillProcess);
            while(g_KillCount < SMP::GetCoreCount()) ;
            g_KillLock.Unlock();
        } else {
            // TODO: Kernel panic (KernelThread has requested to kill itself)
            klog_fatal("PANIC", "KernelThread has requested to kill itself (TID=%i)", Scheduler::ThreadGetTID());
            while(true) 
                asm("hlt");
        }

        ThreadExit(123);
    }

    void ThreadSetSticky() {
        uint64 coreID = SMP::GetLogicalCoreID();

        g_CPUData[coreID].currentThread->stickyCount++;
        if(g_CPUData[coreID].currentThread->stickyCount == 1)
            IDT::DisableInterrupts();
    }
    void ThreadUnsetSticky() {
        uint64 coreID = SMP::GetLogicalCoreID();

        g_CPUData[coreID].currentThread->stickyCount--;
        if(g_CPUData[coreID].currentThread->stickyCount == 0)
            IDT::EnableInterrupts();
    }

    void ThreadSetUnkillable(bool unkillable) {
        uint64 coreID = SMP::GetLogicalCoreID();
        g_CPUData[coreID].currentThread->unkillable = unkillable;

        if(unkillable == false && g_CPUData[coreID].currentThread->killPending) {
            ThreadExit(g_CPUData[coreID].currentThread->killCode);
        }
    }

    extern "C" void ThreadSetPageFaultRip(uint64 rip) {
        g_CPUData[SMP::GetLogicalCoreID()].currentThread->pageFaultRip = rip;
    }
    void ThreadSetupPageFaultHandler(IDT::Registers* regs) {
        uint64 coreID = SMP::GetLogicalCoreID();
        ThreadInfo* tInfo = g_CPUData[coreID].currentThread;

        if(tInfo->pageFaultRip == 0) {
            ThreadKillProcessFromInterrupt(regs, "PageFault");
        } else {
            regs->rip = tInfo->pageFaultRip;
        }
    }

    void MoveThreadToCPU(uint64 logicalCoreID, uint64 tid) {
        IDT::DisableInterrupts();

        uint64 coreID = SMP::GetLogicalCoreID();
        for(auto a = g_CPUData[coreID].threadList.begin(); a != g_CPUData[coreID].threadList.end(); ++a) {
            if(a->tid == tid) {
                g_CPUData[coreID].threadList.erase(a);

                g_TransferLock.SpinLock();
                g_TransferComplete = false;
                g_TransferThread = *a;
                APIC::SendIPI(APIC::IPI_TARGET_CORE, SMP::GetApicID(logicalCoreID), ISRNumbers::IPIMoveThread);
                while(!g_TransferComplete) ;
                g_TransferLock.Unlock();
                IDT::EnableInterrupts();
                return;
            }
        }

        IDT::EnableInterrupts();
    }

    ThreadInfo* GetCurrentThreadInfo() {
        uint64 coreID = SMP::GetLogicalCoreID();
        return g_CPUData[coreID].currentThread;
    }

}