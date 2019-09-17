#include "Scheduler.h"

#include "Thread.h"
#include "multicore/SMP.h"
#include "memory/MemoryManager.h"
#include "arch/GDT.h"
#include "arch/CPU.h"
#include "arch/MSR.h"
#include "arch/APIC.h"
#include "fs/VFS.h"
#include "klib/stdio.h"
#include "arch/SSE.h"
#include "ktl/new.h"
#include "syscalls/SyscallDefine.h"
#include "time/Time.h"
#include "percpu/PerCPU.h"

namespace Scheduler {

    static ThreadInfo* FindNextThread();
    static ThreadInfo* FindNextThreadFromInterrupt();

    extern "C" void GoToThread(IDT::Registers* regs);
    extern "C" void SwitchThread(IDT::Registers* from, IDT::Registers* to);

    struct CPUData {
        uint64 currentThreadKernelStack;

        ThreadMemSpace idleThreadMemSpace;
        ThreadFileDescriptors idleThreadFDs;
        ThreadInfo idleThread;

        ThreadInfo* currentThread;

        StickyLock threadListLock;
        ktl::nlist<ThreadInfo> threadList;
        ktl::nlist<ThreadInfo> deadThreads;

        uint64 lastTSC;
    };

    static DECLARE_PER_CPU(CPUData, g_CPUData);
    static uint64 g_ThreadStructSize;
    static uint64 g_TIDCounter = 1;

    static ThreadInfo* GetNewThreadStruct() {
        auto& cpuData = g_CPUData.Get();

        ThreadInfo* res;

        cpuData.threadListLock.Spinlock_Cli();
        if(!cpuData.deadThreads.empty()) {
            auto t = cpuData.deadThreads.back();
            cpuData.deadThreads.pop_back();
            cpuData.threadListLock.Unlock_Cli();
            res = t;
        } else {
            cpuData.threadListLock.Unlock_Cli();

            char* buf = new char[g_ThreadStructSize];
            res = (ThreadInfo*)buf;

            res->kernelStack = (uint64)new char[KernelStackSize] + KernelStackSize;
        }

        uint64 kStack = res->kernelStack;

        kmemset(res, 0, g_ThreadStructSize);

        res->tid = g_TIDCounter++;
        res->refCount = 1;
        res->kernelStack = kStack;
        res->hasFPUBlock = true;

        return res;
    }

    void MakeMeIdleThread() {
        g_ThreadStructSize = sizeof(ThreadInfo) + SSE::GetFPUBlockSize();

        auto& cpuData = g_CPUData.Get();

        cpuData.idleThreadMemSpace.refCount = 1;
        cpuData.idleThreadMemSpace.pml4Entry = 0;

        cpuData.idleThreadFDs.refCount = 1;

        cpuData.idleThread.refCount = 1;
        cpuData.idleThread.tid = 0;
        cpuData.idleThread.memSpace = &cpuData.idleThreadMemSpace;
        cpuData.idleThread.fds = &cpuData.idleThreadFDs;
        cpuData.idleThread.user = nullptr;
        cpuData.idleThread.blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        cpuData.idleThread.kernelStack = (uint64)MemoryManager::PhysToKernelPtr(MemoryManager::EarlyAllocatePages(KernelStackPages)) + KernelStackSize;
        cpuData.idleThread.userGSBase = 0;
        cpuData.idleThread.userFSBase = 0;
        cpuData.idleThread.stickyCount = 0;
        cpuData.idleThread.cliCount = 1;        // interrupts are disabled when this function is called
        cpuData.idleThread.unkillable = true;
        cpuData.idleThread.killPending = false;
        cpuData.idleThread.killCode = 0;
        cpuData.idleThread.pageFaultRip = 0;
        kmemset(&cpuData.idleThread.registers, 0, sizeof(IDT::Registers));  // will be saved on first scheduler tick, no need to initialize
        cpuData.idleThread.hasFPUBlock = false;

        cpuData.currentThread = &cpuData.idleThread;
    }

    static void RunKernelThread(int64 (*func)(uint64, uint64), uint64 arg1, uint64 arg2) {
        int64 ret = func(arg1, arg2);
        ThreadExit(ret);
    }

    uint64 CreateKernelThread(int64 (*func)(uint64, uint64), uint64 arg1, uint64 arg2) {
        auto tInfo = GetNewThreadStruct();

        auto memSpace = new ThreadMemSpace();
        memSpace->refCount = 1;
        memSpace->pml4Entry = 0;

        auto fds = new ThreadFileDescriptors();
        fds->refCount = 1;

        tInfo->memSpace = memSpace;
        tInfo->fds = fds;

        tInfo->unkillable = true;

        tInfo->registers.cs = GDT::KernelCode;
        tInfo->registers.ss = GDT::KernelData;
        tInfo->registers.ds = GDT::KernelData;
        tInfo->registers.userrsp = tInfo->kernelStack;
        tInfo->registers.rip = (uint64)&RunKernelThread;
        tInfo->registers.rflags = CPU::FLAGS_IF;
        tInfo->registers.rdi = (uint64)func;
        tInfo->registers.rsi = arg1;
        tInfo->registers.rdx = arg2;

        auto& cpuData = g_CPUData.Get();
        cpuData.threadListLock.Spinlock_Cli();
        cpuData.threadList.push_back(tInfo);
        cpuData.threadListLock.Unlock_Cli();

        return tInfo->tid;
    }

    static ThreadInfo* CloneThread(bool shareMemSpace, bool shareFDs) {
        auto& cpuData = g_CPUData.Get();

        auto tInfo = cpuData.currentThread;
        auto newT = GetNewThreadStruct();
        newT->refCount = 2;

        ThreadMemSpace* memSpace;
        if(shareMemSpace) {
            memSpace = tInfo->memSpace;
            memSpace->refCount.Inc();
        } else {
            memSpace = new ThreadMemSpace();
            memSpace->refCount = 1;
            memSpace->pml4Entry = MemoryManager::ForkProcessMap();
        }

        ThreadFileDescriptors* fds;
        if(shareFDs) {
            fds = tInfo->fds;
            fds->refCount.Inc();
        } else {
            fds = new ThreadFileDescriptors();
            fds->refCount = 1;

            tInfo->fds->lock.Spinlock();
            for(const auto& fd : tInfo->fds->fds) {
                VFS::AddRef(fd.sysDesc);
                fds->fds.push_back(fd);
            }
            tInfo->fds->lock.Unlock();
        }

        tInfo->children.push_back(newT);

        return newT;
    }

    void ThreadDetach(uint64 tid) {
        auto tInfo = g_CPUData.Get().currentThread;

        for(auto a = tInfo->children.begin(); a != tInfo->children.end(); ) {
            if((*a)->tid == tid) {
                if((*a)->refCount.DecAndCheckZero()) {
                    g_CPUData.Get().threadListLock.Spinlock_Cli();
                    g_CPUData.Get().deadThreads.push_back(*a);
                    g_CPUData.Get().threadListLock.Unlock_Cli();
                }

                tInfo->children.erase(a);
                return;
            } else {
                ++a;
            }
        }
    }

    bool ThreadGetChildInfo(uint64 tid, int64& exitCode) {
        auto tInfo = g_CPUData.Get().currentThread;

        for(auto c : tInfo->children) {
            if(c->tid == tid) {
                exitCode = c->exitCode;
                return c->exited;
            }
        }
        return false;
    }

    static void SaveThreadState(ThreadInfo* tInfo) {
        if(tInfo->hasFPUBlock)
            SSE::SaveFPUBlock(tInfo->fpuState);
    }
    static void SaveThreadStateAndRegs(ThreadInfo* tInfo, IDT::Registers* regs) {
        tInfo->registers = *regs;
        SaveThreadState(tInfo);
    }

    static void RestoreThreadState(ThreadInfo* tInfo) {
        if(tInfo->hasFPUBlock)
            SSE::RestoreFPUBlock(tInfo->fpuState);
        
        MemoryManager::SwitchProcessMap(tInfo->memSpace->pml4Entry);

        g_CPUData.Get().currentThreadKernelStack = tInfo->kernelStack;

        bool kernelMode = (tInfo->registers.cs & 0x3) == 0;
        uint64 kernelGS = (uint64)&g_CPUData.Get();
        MSR::Write(MSR::RegGSBase, kernelMode ? kernelGS : tInfo->userGSBase);
        MSR::Write(MSR::RegKernelGSBase, kernelMode ? tInfo->userGSBase : kernelGS);
        MSR::Write(MSR::RegFSBase, tInfo->userFSBase);
    }
    static void RestoreThreadStateAndRegs(ThreadInfo* tInfo, IDT::Registers* regs) {
        *regs = tInfo->registers;
        RestoreThreadState(tInfo);
    }

    static ThreadInfo* FindNextThreadFromInterrupt() {
        g_CPUData.Get().threadListLock.Spinlock_Raw();
        for(auto t : g_CPUData.Get().threadList) {
            if(t->blockEvent.type == ThreadBlockEvent::TYPE_NONE) {
                g_CPUData.Get().threadList.erase(t);
                g_CPUData.Get().threadList.push_back(t);
                g_CPUData.Get().threadListLock.Unlock_Raw();
                return t;
            }
        }
        g_CPUData.Get().threadListLock.Unlock_Raw();
        return &g_CPUData.Get().idleThread;
    }

    static ThreadInfo* FindNextThread() {
        g_CPUData.Get().threadListLock.Spinlock_Cli();
        for(auto t : g_CPUData.Get().threadList) {
            if(t->blockEvent.type == ThreadBlockEvent::TYPE_NONE) {
                g_CPUData.Get().threadList.erase(t);
                g_CPUData.Get().threadList.push_back(t);
                g_CPUData.Get().threadListLock.Unlock_Cli();
                return t;
            }
        }
        g_CPUData.Get().threadListLock.Unlock_Cli();
        return &g_CPUData.Get().idleThread;
    }

    void Tick(IDT::Registers* regs) {
        auto current = g_CPUData.Get().currentThread;

        auto next = FindNextThreadFromInterrupt();
        if(next == current)
            return;

        SaveThreadStateAndRegs(current, regs);
        RestoreThreadStateAndRegs(next, regs);
        g_CPUData.Get().currentThread = next;
    }

    void ThreadYield() {
        ThreadDisableInterrupts();
        
        auto next = FindNextThread();
        if(next == &g_CPUData.Get().idleThread) {
            ThreadEnableInterrupts();
            return;
        }

        auto current = g_CPUData.Get().currentThread;

        SaveThreadState(current);
        RestoreThreadState(next);

        SwitchThread(&current->registers, &next->registers);

        ThreadEnableInterrupts();
    }

    void ThreadSleep(uint64 ms) {
        ThreadDisableInterrupts();

        auto tInfo = g_CPUData.Get().currentThread;

        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_WAIT;
        tInfo->blockEvent.wait.remainingTicks = Time::GetTSCTicksPerMilli() * ms;

        ThreadYield();
        ThreadEnableInterrupts();
    }

    static void TerminateLocalThread(ThreadInfo* tInfo) {
        if(tInfo->refCount.Read() == 1) {
            g_CPUData.Get().threadList.erase(tInfo);
            g_CPUData.Get().deadThreads.push_back(tInfo);
        } else {
            if(tInfo->unkillable) {
                tInfo->killCode = 0;
                tInfo->killPending = true;
            } else {
                tInfo->registers.cs = GDT::KernelCode;
                tInfo->registers.ss = GDT::KernelData;
                tInfo->registers.ds = GDT::KernelData;
                tInfo->registers.userrsp = tInfo->kernelStack;
                tInfo->registers.rip = (uint64)&ThreadExit;
                tInfo->registers.rdi = 1;
            }
        }
    }

    static void TerminateThreadGroup(ThreadInfo* parent) {
        auto& cpuData = g_CPUData.Get();

        cpuData.threadListLock.Spinlock_Cli();
        for(auto a = cpuData.threadList.begin(); a != cpuData.threadList.end(); ) {
            if(a->parent == parent) {
                auto thread = *a;
                ++a;
                TerminateLocalThread(thread);
            } else {
                ++a;
            }
        }
        cpuData.threadListLock.Unlock_Cli();


    }

    void ThreadExit(uint64 code) {
        auto tInfo = g_CPUData.Get().currentThread;

        if(tInfo->memSpace->refCount.DecAndCheckZero()) {
            MemoryManager::FreeProcessMap(tInfo->memSpace->pml4Entry);
            delete tInfo->memSpace;
        }

        if(tInfo->fds->refCount.DecAndCheckZero()) {
            for(auto fd : tInfo->fds->fds) {
                if(fd.sysDesc != 0)
                    VFS::Close(fd.sysDesc);
            }
            delete tInfo->fds;
        }

        TerminateThreadGroup(tInfo);

        ThreadDisableInterrupts();

        if(tInfo->refCount.DecAndCheckZero()) {
            g_CPUData.Get().threadListLock.Spinlock_Cli();
            g_CPUData.Get().threadList.erase(tInfo);
            g_CPUData.Get().deadThreads.push_back(tInfo);
            g_CPUData.Get().threadListLock.Unlock_Cli();
        }

        auto next = FindNextThread();
        RestoreThreadState(next);
        GoToThread(&next->registers);
    }



}