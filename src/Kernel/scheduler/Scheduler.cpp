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

    extern "C" void GoToThread(IDT::Registers* regs);
    extern "C" int64 SwitchThread(IDT::Registers* from, IDT::Registers* to);

    struct CPUData {
        uint64 currentThreadKernelStack;

        ThreadMemSpace idleThreadMemSpace;
        ThreadFileDescriptors idleThreadFDs;
        ThreadInfo idleThread;

        ThreadInfo* currentThread;

        StickyLock activeListLock;
        ktl::nlist<ThreadInfo> activeList;

        StickyLock orphanLock;

        uint64 lastTSC;
    };

    static DECLARE_PER_CPU(CPUData, g_CPUData);
    static uint64 g_ThreadStructSize;
    static uint64 g_TIDCounter = 1;

    static ThreadInfo* FindNextThread() {
        auto& cpuData = g_CPUData.Get();

        for(auto a = cpuData.activeList.begin(); a != cpuData.activeList.end(); ++a) {
            if((*a)->state.type == ThreadState::STATE_READY) {
                cpuData.activeList.erase(a);
                cpuData.activeList.push_back(*a);
                return *a;
            }
        }

        return &cpuData.idleThread;
    }

    static void SaveThreadState(ThreadInfo* tInfo) {
        if(tInfo->hasFPUBlock)
            SSE::SaveFPUBlock(tInfo->fpuBuffer);
    }
    static void SaveThreadStateAndRegs(ThreadInfo* tInfo, IDT::Registers* regs) {
        tInfo->registers = *regs;
        SaveThreadState(tInfo);
    }

    static void LoadThreadState(ThreadInfo* tInfo) {
        if(tInfo->hasFPUBlock)
            SSE::RestoreFPUBlock(tInfo->fpuBuffer);

        MemoryManager::SwitchProcessMap(tInfo->memSpace->pml4Entry);

        auto& cpuData = g_CPUData.Get();

        bool kernelMode = tInfo->registers.cs & 0x3 == 0;
        uint64 kernelGS = (uint64)&cpuData;
        MSR::Write(MSR::RegGSBase, kernelMode ? kernelGS : tInfo->userGSBase);
        MSR::Write(MSR::RegKernelGSBase, kernelMode ? tInfo->userGSBase : kernelGS);
        MSR::Write(MSR::RegFSBase, tInfo->userFSBase);

        cpuData.currentThreadKernelStack = tInfo->kernelStack;
        cpuData.currentThread = tInfo;
    }
    static void LoadThreadStateAndRegs(ThreadInfo* tInfo, IDT::Registers* regs) {
        *regs = tInfo->registers;
        LoadThreadState(tInfo);
    }

    static void TimerEvent(IDT::Registers* regs) {
        Tick(regs);
    }

    void MakeMeIdleThread() {
        APIC::SetTimerEvent(TimerEvent);

        auto& cpuData = g_CPUData.Get();

        cpuData.idleThreadMemSpace.refCount = 1;
        cpuData.idleThreadMemSpace.pml4Entry = 0;

        cpuData.idleThreadFDs.refCount = 1;
        
        cpuData.idleThread.tid = 0;
        cpuData.idleThread.exitCode = 0;
        cpuData.idleThread.killPending = false;
        cpuData.idleThread.memSpace = &cpuData.idleThreadMemSpace;
        cpuData.idleThread.fds = &cpuData.idleThreadFDs;
        cpuData.idleThread.user = nullptr;
        cpuData.idleThread.state.type = ThreadState::STATE_READY;
        cpuData.idleThread.kernelStack = (uint64)MemoryManager::PhysToKernelPtr(MemoryManager::EarlyAllocatePages(KernelStackPages)) + KernelStackSize;
        cpuData.idleThread.userGSBase = 0;
        cpuData.idleThread.userFSBase = 0;
        cpuData.idleThread.stickyCount = 0;
        cpuData.idleThread.cliCount = 1;        // Interrupts should be disabled when this function is called
        cpuData.idleThread.pageFaultRip = 0;
        cpuData.idleThread.registers = { 0 };   // will be filled on first scheduler tick
        cpuData.idleThread.hasFPUBlock = false;

        cpuData.currentThread = &cpuData.idleThread;
    }

    static void RunKernelThread(int64 (*func)(uint64, uint64), uint64 arg1, uint64 arg2) {
        ThreadExit(func(arg1, arg2));
    }

    uint64 CreateKernelThread(int64 (*func)(uint64, uint64), uint64 arg1, uint64 arg2) {
        auto memSpace = new ThreadMemSpace();
        memSpace->refCount = 1;
        memSpace->pml4Entry = 0;

        auto fds = new ThreadFileDescriptors();
        fds->refCount = 1;

        auto tInfo = new ThreadInfo();
        tInfo->tid = g_TIDCounter++;
        tInfo->exitCode = 0;
        tInfo->killPending = false;
        tInfo->memSpace = memSpace;
        tInfo->fds = fds;
        tInfo->user = nullptr;
        tInfo->state.type = ThreadState::STATE_READY;
        tInfo->kernelStack = (uint64)new char[KernelStackSize] + KernelStackSize;
        tInfo->userGSBase = 0;
        tInfo->userFSBase = 0;
        tInfo->stickyCount = 0;
        tInfo->cliCount = 0;
        tInfo->pageFaultRip = 0;
        tInfo->registers.cs = GDT::KernelCode;
        tInfo->registers.ds = GDT::KernelData;
        tInfo->registers.ss = GDT::KernelData;
        tInfo->registers.rip = (uint64)&RunKernelThread;
        tInfo->registers.userrsp = tInfo->kernelStack;
        tInfo->registers.rdi = (uint64)func;
        tInfo->registers.rsi = arg1;
        tInfo->registers.rdx = arg2;

        g_CPUData.Get().orphanLock.Spinlock();
        g_CPUData.Get().idleThread.joinThreads.push_back(tInfo);
        g_CPUData.Get().orphanLock.Unlock();

        g_CPUData.Get().activeListLock.Spinlock_Cli();
        g_CPUData.Get().activeList.push_back(tInfo);
        g_CPUData.Get().activeListLock.Unlock_Cli();

        return tInfo->tid;
    }

    int64 CloneThread(bool shareMemSpace, bool shareFDs, IDT::Registers* regs) {
        auto tInfo = g_CPUData.Get().currentThread;

        ThreadSetSticky();

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
            for(auto fd : tInfo->fds->fds) {
                if(fd.sysDesc != 0) {
                    fds->fds.push_back(fd);
                    VFS::AddRef(fd.sysDesc);
                }
            }
            tInfo->fds->lock.Unlock();
        }

        auto newT = new ThreadInfo();
        newT->tid = g_TIDCounter++;
        newT->killPending = false;
        newT->memSpace = memSpace;
        newT->fds = fds;
        newT->user = tInfo->user;
        newT->state.type = ThreadState::STATE_READY;
        newT->kernelStack = (uint64)new char[KernelStackSize] + KernelStackSize;
        newT->userGSBase = tInfo->userGSBase;
        newT->userFSBase = tInfo->userFSBase;
        newT->stickyCount = 0;
        newT->cliCount = 0;
        newT->pageFaultRip = 0;
        newT->registers = *regs;
        newT->hasFPUBlock = false;

        tInfo->joinThreads.push_back(newT);

        g_CPUData.Get().activeListLock.Spinlock_Cli();
        g_CPUData.Get().activeList.push_back(newT);
        g_CPUData.Get().activeListLock.Unlock_Cli();

        ThreadUnsetSticky();

        return newT->tid;
    }

    void ThreadDetach(uint64 tid) {
        auto& cpuData = g_CPUData.Get();
        auto tInfo = cpuData.currentThread;

        for(auto a = tInfo->joinThreads.begin(); a != tInfo->joinThreads.end(); ++a) {
            if((*a)->tid == tid) {
                tInfo->joinThreads.erase(a);

                cpuData.orphanLock.Spinlock();
                cpuData.idleThread.joinThreads.push_back(*a);
                cpuData.orphanLock.Unlock();

                return;
            }
        }
    }

    // return 0:                yield successful (thread block state resolved)
    // return anything else:    yield unsuccessful (thread block state interrupted)
    int64 _Block() {
        auto& cpuData = g_CPUData.Get();
        auto tInfo = cpuData.currentThread;

        cpuData.activeListLock.Spinlock_Cli();
        auto next = FindNextThread();
        cpuData.activeListLock.Unlock_Cli();

        if(next == tInfo)
            next = &cpuData.idleThread;

        SaveThreadState(tInfo);
        LoadThreadState(next);
        return SwitchThread(&tInfo->registers, &next->registers);
    }

    bool ThreadJoin(uint64 tid, int64& exitCode) {
        auto tInfo = g_CPUData.Get().currentThread;

        ThreadInfo* jt = nullptr;
        for(auto a = tInfo->joinThreads.begin(); a != tInfo->joinThreads.end(); ++a) {
            if((*a)->tid == tid) {
                jt = *a;
                tInfo->joinThreads.erase(a);
                break;
            }
        }

        if(jt == nullptr)
            return false;

        ThreadSetSticky();
        tInfo->state.type = ThreadState::STATE_JOIN;
        tInfo->state.join.joinThread = jt;
        
        if(_Block()) {
            ThreadUnsetSticky();

            if(tInfo->killPending) {
                ThreadExit(0);
            }

            return false;
        }

        ThreadUnsetSticky();

        exitCode = jt->exitCode;
        delete[] (char*)(jt->kernelStack - KernelStackSize);
        delete jt;
        return true;
    }

    static void UpdateStates() {
        auto& cpuData = g_CPUData.Get();

        auto tsc = Time::GetTSC();
        auto dur = tsc - cpuData.lastTSC;
        cpuData.lastTSC = tsc;

        for(ThreadInfo* tInfo : cpuData.activeList) {
            if(tInfo->state.type == ThreadState::STATE_EXITED) {
                tInfo->state.type = ThreadState::STATE_DEAD;
            }
            if(tInfo->state.type == ThreadState::STATE_DEAD)
                continue;

            if(tInfo->killPending) {
                tInfo->registers.rax = 1;
                tInfo->state.type = ThreadState::STATE_READY;
            } else if(tInfo->state.type == ThreadState::STATE_WAIT) {
                if(tInfo->state.wait.remainingTicks <= dur) {
                    tInfo->state.type = ThreadState::STATE_READY;
                    tInfo->registers.rax = 0;
                } else {
                    tInfo->state.wait.remainingTicks -= dur;
                }
            } else if(tInfo->state.type == ThreadState::STATE_JOIN) {
                if(tInfo->state.join.joinThread->state.type == ThreadState::STATE_DEAD) {
                    tInfo->state.type = ThreadState::STATE_READY;
                    tInfo->registers.rax = 0;
                }
            }
        }
    }

    void Tick(IDT::Registers* regs) {
        auto& cpuData = g_CPUData.Get();

        if(cpuData.currentThread->stickyCount != 0)
            return;

        cpuData.activeListLock.Spinlock_Raw();
        UpdateStates();
        auto next = FindNextThread();
        cpuData.activeListLock.Unlock_Raw();

        if(next == cpuData.currentThread)
            return;

        SaveThreadStateAndRegs(cpuData.currentThread, regs);
        LoadThreadStateAndRegs(next, regs);
    }

    void ThreadYield() {
        auto& cpuData = g_CPUData.Get();
        auto tInfo = cpuData.currentThread;

        ThreadSetSticky();

        cpuData.activeListLock.Spinlock_Cli();
        auto next = FindNextThread();
        cpuData.activeListLock.Unlock_Cli();

        if(next == tInfo) {
            ThreadUnsetSticky();
            return;
        }

        SaveThreadState(tInfo);
        LoadThreadState(next);
        SwitchThread(&tInfo->registers, &next->registers);

        ThreadUnsetSticky();
    }

    void ThreadSleep(uint64 ms) {
        auto tInfo = g_CPUData.Get().currentThread;

        ThreadSetSticky();
        tInfo->state.type = ThreadState::STATE_WAIT;
        tInfo->state.wait.remainingTicks = ms * Time::GetTSCTicksPerMilli();

        ThreadYield();
        ThreadUnsetSticky();
    }

    void ThreadExit(uint64 code) {
        auto& cpuData = g_CPUData.Get();
        auto tInfo = cpuData.currentThread;

        cpuData.orphanLock.Spinlock();
        for(auto jt : tInfo->joinThreads) {
            cpuData.idleThread.joinThreads.push_back(jt);
            jt->killPending = true;
        }
        cpuData.orphanLock.Unlock();

        ThreadSetSticky();

        if(tInfo->memSpace->refCount.DecAndCheckZero()) {
            if(tInfo->memSpace->pml4Entry != 0)
                MemoryManager::FreeProcessMap(tInfo->memSpace->pml4Entry);
            delete tInfo->memSpace;
        }

        if(tInfo->fds->refCount.DecAndCheckZero()) {
            for(auto fd : tInfo->fds->fds)
                if(fd.sysDesc != 0)
                    VFS::Close(fd.sysDesc);
            delete tInfo->fds;
        }

        tInfo->exitCode = code;
        tInfo->state.type = ThreadState::STATE_EXITED;

        cpuData.activeListLock.Spinlock_Cli();
        cpuData.activeList.erase(tInfo);
        cpuData.activeListLock.Unlock_Cli();

        ThreadYield();
    }

    uint64 ThreadGetTID() {
        return g_CPUData.Get().currentThread->tid;
    }

    uint64 ThreadGetUID() {
        auto user = g_CPUData.Get().currentThread->user;
        if(user == nullptr)
            return 0;
        return user->uid;
    }

    uint64 ThreadGetGID() {
        auto user = g_CPUData.Get().currentThread->user;
        if(user == nullptr)
            return 0;
        return user->gid;
    }

    const char* ThreadGetUserName() {
        auto user = g_CPUData.Get().currentThread->user;
        if(user == nullptr)
            return "--KernelThread--";
        return user->name;
    }

    static ThreadFileDescriptor* GetFreeFD(ThreadInfo* tInfo) {
        int64 lastID = -1;
        for(auto a = tInfo->fds->fds.begin(); a != tInfo->fds->fds.end(); ++a) {
            if(a->id != lastID + 1) {
                auto res = tInfo->fds->fds.insert(a, { lastID + 1, 0 });
                return &*res;
            } else if(a->sysDesc == 0) {
                return &*a;
            }

            lastID = a->id;
        }

        tInfo->fds->fds.push_back({ lastID + 1, 0 });
        return &tInfo->fds->fds.back();
    }

    int64 ThreadAddFileDescriptor(uint64 sysDesc) {
        auto tInfo = g_CPUData.Get().currentThread;

        tInfo->fds->lock.Spinlock();
        auto fd = GetFreeFD(tInfo);
        fd->sysDesc = sysDesc;
        int64 ret = fd->id;
        tInfo->fds->lock.Unlock();
        return ret;
    }

    int64 ThreadReplaceFileDescriptor(int64 oldPDesc, int64 newPDesc) {
        auto tInfo = g_CPUData.Get().currentThread;

        if(oldPDesc == newPDesc)
            return OK;
        if(oldPDesc < 0)
            return ErrorInvalidFD;

        ThreadFileDescriptor* oldFD;
        ThreadFileDescriptor* newFD;
        tInfo->fds->lock.Spinlock();
        for(auto& fd : tInfo->fds->fds) {
            if(fd.id == oldPDesc)
                oldFD = &fd;
            if(fd.id == newPDesc)
                newFD = &fd;
        }

        if(oldFD == nullptr || newFD == nullptr || newFD->sysDesc == 0) {
            tInfo->fds->lock.Unlock();
            return ErrorInvalidFD;
        }
        if(oldFD->sysDesc == newFD->sysDesc) {
            tInfo->fds->lock.Unlock();
            return OK;
        }

        if(oldFD->sysDesc != 0) {
            VFS::Close(oldFD->sysDesc);
        }

        VFS::AddRef(newFD->sysDesc);
        oldFD->sysDesc = newFD->sysDesc;

        tInfo->fds->lock.Unlock();
        return OK;
    }

    int64 ThreadCloseFileDescriptor(int64 desc) {
        auto tInfo = g_CPUData.Get().currentThread;

        tInfo->fds->lock.Spinlock();
        for(auto& fd : tInfo->fds->fds) {
            if(fd.id == desc) {
                uint64 sysDesc = fd.sysDesc;
                fd.sysDesc = 0;
                tInfo->fds->lock.Unlock();
                return VFS::Close(sysDesc);
            }
        }
        tInfo->fds->lock.Unlock();

        return ErrorInvalidFD;
    }

    int64 ThreadGetSystemFileDescriptor(int64 pDesc, uint64& sysDesc) {
        auto tInfo = g_CPUData.Get().currentThread;

        tInfo->fds->lock.Spinlock();
        for(auto& fd : tInfo->fds->fds) {
            if(fd.id == pDesc) {
                sysDesc = fd.sysDesc;
                tInfo->fds->lock.Unlock();
                return OK;
            }
        }
        tInfo->fds->lock.Unlock();

        return ErrorInvalidFD;
    }

    void ThreadSetFS(uint64 val) {
        g_CPUData.Get().currentThread->userFSBase = val;
        MSR::Write(MSR::RegFSBase, val);
    }
    void ThreadSetGS(uint64 val) {
        g_CPUData.Get().currentThread->userGSBase = val;
        MSR::Write(MSR::RegKernelGSBase, val);
    }

    void ThreadExec(uint64 pml4Entry, IDT::Registers* regs) {
        auto tInfo = g_CPUData.Get().currentThread;

        ThreadSetSticky();

        if(tInfo->memSpace->refCount.DecAndCheckZero()) {
            if(tInfo->memSpace->pml4Entry != 0)
                MemoryManager::FreeProcessMap(tInfo->memSpace->pml4Entry);
            tInfo->memSpace->pml4Entry = pml4Entry;
            tInfo->memSpace->refCount = 1;
        } else {
            auto memSpace = new ThreadMemSpace();
            memSpace->pml4Entry = pml4Entry;
            memSpace->refCount = 1;
            tInfo->memSpace = memSpace;
        }

        MemoryManager::SwitchProcessMap(tInfo->memSpace->pml4Entry);

        ThreadDisableInterrupts();

        SaveThreadStateAndRegs(tInfo, regs);
        tInfo->cliCount = 0;
        tInfo->stickyCount = 0;

        g_CPUData.Get().activeListLock.Spinlock_Cli();
        auto next = FindNextThread();
        g_CPUData.Get().activeListLock.Unlock_Cli();

        LoadThreadState(next);
        GoToThread(&next->registers);
    }

    void ThreadSetSticky() {
        g_CPUData.Get().currentThread->stickyCount++;
    }
    void ThreadUnsetSticky() {
        g_CPUData.Get().currentThread->stickyCount--;
    }

    void ThreadDisableInterrupts() {
        g_CPUData.Get().currentThread->cliCount++;
        if(g_CPUData.Get().currentThread->cliCount == 1)
            IDT::DisableInterrupts();
    }
    void ThreadEnableInterrupts() {
        g_CPUData.Get().currentThread->cliCount--;
        if(g_CPUData.Get().currentThread->cliCount == 0)
            IDT::EnableInterrupts();
    }

    void ThreadSetUnkillable(bool k) {
        if(k == false) {
            if(g_CPUData.Get().currentThread->killPending)
                ThreadExit(1);
        }
    }

    extern "C" void ThreadSetPageFaultRip(uint64 val) {
        g_CPUData.Get().currentThread->pageFaultRip = val;
    }

    void ThreadSetupPageFaultHandler(IDT::Registers* regs) {
        auto tInfo = g_CPUData.Get().currentThread;

        if(tInfo->pageFaultRip != 0)
            tInfo->registers.rip = tInfo->pageFaultRip;
        else {
            tInfo->registers.rip = (uint64)&ThreadExit;
            tInfo->registers.rdi = 1;
        }
    }

    ThreadInfo* GetCurrentThreadInfo() {
        return g_CPUData.Get().currentThread;
    }

}