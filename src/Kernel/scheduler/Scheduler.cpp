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

        uint64 lastTSC;
    };

    static DECLARE_PER_CPU(CPUData, g_CPUData);
    static uint64 g_ThreadStructSize;
    static Atomic<uint64> g_TIDCounter = 1;

    static ThreadInfo* g_InitThread = nullptr;

    static void TimerEvent(IDT::Registers* regs) {

    }

    static ThreadInfo* FindNextThread() {
        auto& cpuData = g_CPUData.Get();

        auto tsc = Time::GetTSC();
        auto dur = tsc - cpuData.lastTSC;
        cpuData.lastTSC = tsc;

        for(auto a = cpuData.activeList.begin(); a != cpuData.activeList.end(); ++a) {
            auto tInfo = *a;

            if(tInfo->state.type == ThreadState::FINISHED || tInfo->state.type == ThreadState::EXITED) {
                tInfo->state.type = ThreadState::EXITED;
                continue;
            }

            if(tInfo->killPending) {
                tInfo->state.type = ThreadState::READY;
                tInfo->registers.rax = 1;
            } else if(tInfo->state.type == ThreadState::WAIT) {
                if(tInfo->state.arg <= dur) {
                    tInfo->state.type = ThreadState::READY;
                    tInfo->registers.rax = 0;
                } else {
                    tInfo->state.arg -= dur;
                }
            } else if(tInfo->state.type == ThreadState::JOIN) {
                auto jt = (ThreadInfo*)tInfo->state.arg;
                if(jt->state.type == ThreadState::EXITED) {
                    tInfo->state.type = ThreadState::READY;
                    tInfo->registers.rax = 0;
                }
            }

            if(tInfo->state.type == ThreadState::READY) {
                cpuData.activeList.erase(a);
                return tInfo;
            }
        }
    }

    static void SaveThreadState(ThreadInfo* tInfo) {
        if(tInfo->fpuBuffer != nullptr)
            SSE::SaveFPUBlock(tInfo->fpuBuffer);
    }
    static void LoadThreadState(ThreadInfo* tInfo) {
        auto& cpuData = g_CPUData.Get();

        if(tInfo->fpuBuffer != nullptr)
            SSE::RestoreFPUBlock(tInfo->fpuBuffer);
        
        if(tInfo->memSpace->pml4Entry != 0)
            MemoryManager::SwitchProcessMap(tInfo->memSpace->pml4Entry);

        bool kernelMode = (tInfo->registers.cs & 0x3) == 0;
        uint64 kernelGS = (uint64)&cpuData.currentThreadKernelStack;
        MSR::Write(MSR::RegGSBase, kernelMode ? kernelGS : tInfo->userGSBase);
        MSR::Write(MSR::RegKernelGSBase, kernelMode ? tInfo->userGSBase : kernelGS);
        MSR::Write(MSR::RegFSBase, tInfo->userFSBase);

        cpuData.currentThreadKernelStack = tInfo->kernelStack;
        cpuData.currentThread = tInfo;
    }
    static void SaveThreadStateAndRegs(ThreadInfo* tInfo, IDT::Registers* regs) {
        tInfo->registers = *regs;
        SaveThreadState(tInfo);
    }
    static void LoadThreadStateAndRegs(ThreadInfo* tInfo, IDT::Registers* regs) {
        *regs = tInfo->registers;
        LoadThreadState(tInfo);
    }

    void MakeMeIdleThread() {
        APIC::SetTimerEvent(TimerEvent);

        auto& cpuData = g_CPUData.Get();

        cpuData.idleThreadMemSpace.refCount = 1;
        cpuData.idleThreadMemSpace.pml4Entry = 0;
        
        cpuData.idleThreadFDs.refCount = 1;
        
        cpuData.idleThread.mainThread = &cpuData.idleThread;
        cpuData.idleThread.tid = 0;
        cpuData.idleThread.killPending = false;
        cpuData.idleThread.memSpace = &cpuData.idleThreadMemSpace;
        cpuData.idleThread.fds = &cpuData.idleThreadFDs;
        cpuData.idleThread.user = nullptr;
        cpuData.idleThread.state.type = ThreadState::READY;
        cpuData.idleThread.stickyCount = 1;
        cpuData.idleThread.cliCount = 1;
        cpuData.idleThread.faultRip = 0;
        cpuData.idleThread.fpuBuffer = nullptr;

        cpuData.currentThread = &cpuData.idleThread;
    }

    static ThreadInfo* _CreateKernelThread(int64 (*func)(uint64, uint64), uint64 arg1 = 0, uint64 arg2 = 0) {
        auto memSpace = new ThreadMemSpace();
        memSpace->pml4Entry = 0;
        memSpace->refCount = 1;

        auto fds = new ThreadFileDescriptors();
        fds->refCount = 1;

        auto tInfo = new ThreadInfo();
        tInfo->mainThread = tInfo;
        tInfo->tid = g_TIDCounter++;
        tInfo->killPending = false;
        tInfo->memSpace = memSpace;
        tInfo->fds = fds;
        tInfo->user = nullptr;
        tInfo->state.type = ThreadState::READY;
        tInfo->kernelStack = (uint64)new char[KernelStackSize] + KernelStackSize;
        tInfo->stickyCount = 0;
        tInfo->cliCount = 0;
        tInfo->faultRip = 0;
        tInfo->fpuBuffer = nullptr;

        tInfo->registers.cs = GDT::KernelCode;
        tInfo->registers.ds = GDT::KernelData;
        tInfo->registers.ss = GDT::KernelData;
        tInfo->registers.rflags = CPU::FLAGS_IF;
        tInfo->registers.rip = (uint64)func;
        tInfo->registers.rdi = arg1;
        tInfo->registers.rsi = arg2;
        tInfo->registers.userrsp = tInfo->kernelStack;

        g_CPUData.Get().activeListLock.Spinlock_Cli();
        g_CPUData.Get().activeList.push_back(tInfo);
        g_CPUData.Get().activeListLock.Unlock_Cli();

        return tInfo;
    }

    void CreateInitKernelThread(int64 (*func)(uint64, uint64)) {
        auto tInfo = _CreateKernelThread(func);
        g_InitThread = tInfo;
    }

    int64 CreateKernelThread(int64 (*func)(uint64, uint64), uint64 arg1, uint64 arg2) {
        auto tInfo = _CreateKernelThread(func, arg1, arg2);

        g_InitThread->joinThreadsLock.Spinlock();
        g_InitThread->joinThreads.push_back(tInfo);
        g_InitThread->joinThreadsLock.Unlock();

        return tInfo->tid;
    }

    int64 CloneThread(bool subthread, bool shareMemSpace, bool shareFDs, IDT::Registers* regs) {
        auto& cpuData = g_CPUData.Get();
        auto tInfo = cpuData.currentThread;

        ThreadMemSpace* memSpace;
        if(shareMemSpace) {
            memSpace = tInfo->memSpace;
            memSpace->refCount++;
        } else {
            memSpace = new ThreadMemSpace();
            memSpace->refCount = 1;
            memSpace->pml4Entry = MemoryManager::ForkProcessMap();
        }

        ThreadFileDescriptors* fds;
        if(shareFDs) {
            fds = tInfo->fds;
            fds->refCount++;
        } else {
            fds = new ThreadFileDescriptors();
            fds->refCount = 1;

            tInfo->fds->lock.Spinlock();
            for(const auto& fd : tInfo->fds->fds) {
                if(fd.sysDesc != 0) {
                    fds->fds.push_back({ fd });
                    VFS::AddRef(fd.sysDesc);
                }
            }
            tInfo->fds->lock.Unlock();
        }

        auto newT = new ThreadInfo();
        newT->mainThread = subthread ? tInfo->mainThread : newT;
        newT->tid = g_TIDCounter++;
        newT->killPending = false;
        newT->memSpace = memSpace;
        newT->fds = fds;
        newT->user = tInfo->user;
        newT->state.type = ThreadState::READY;
        newT->kernelStack = (uint64)new char[KernelStackSize] + KernelStackSize;
        newT->stickyCount = 0;
        newT->cliCount = 0;
        newT->faultRip = 0;
        newT->registers = *regs;
        newT->registers.rflags |= CPU::FLAGS_IF;
        newT->fpuBuffer = new char[SSE::GetFPUBlockSize()];

        tInfo->mainThread->joinThreadsLock.Spinlock();
        tInfo->mainThread->joinThreads.push_back(newT);
        tInfo->mainThread->joinThreadsLock.Unlock();

        cpuData.activeListLock.Spinlock_Cli();
        cpuData.activeList.push_back(newT);
        cpuData.activeListLock.Unlock_Cli();

        return newT->tid;
    }

    int64 ThreadBlock(ThreadState::Type type, uint64 arg) {
        auto& cpuData = g_CPUData.Get();
        auto tInfo = cpuData.currentThread;
        
        ThreadSetSticky();

        tInfo->state.type = type;
        tInfo->state.arg = arg;

        cpuData.activeListLock.Spinlock_Cli();
        auto next = FindNextThread();
        cpuData.activeListLock.Unlock_Cli();

        SaveThreadState(tInfo);
        LoadThreadState(next);

        int64 res = SwitchThread(&tInfo->registers, &next->registers);
        ThreadUnsetSticky();
        return res;
    }

    int64 ThreadDetach(int64 tid) {
        auto mainThread = g_CPUData.Get().currentThread->mainThread;

        ThreadInfo* detachThread = nullptr;

        mainThread->joinThreadsLock.Spinlock();
        for(auto a = mainThread->joinThreads.begin(); a != mainThread->joinThreads.end(); ++a) {
            if((*a)->tid == tid) {
                detachThread = *a;
                mainThread->joinThreads.erase(a);
                break;
            }
        }
        mainThread->joinThreadsLock.Unlock();

        if(detachThread == nullptr)
            return ErrorThreadNotFound;

        detachThread->mainThread = detachThread;

        g_InitThread->joinThreadsLock.Spinlock();
        g_InitThread->joinThreads.push_back(detachThread);
        g_InitThread->joinThreadsLock.Unlock();

        return OK;
    }

    int64 ThreadJoin(int64 tid, int64& exitCode) {
        auto mainThread = g_CPUData.Get().currentThread->mainThread;

        ThreadInfo* joinThread = nullptr;
        mainThread->joinThreadsLock.Spinlock();
        for(auto a = mainThread->joinThreads.begin(); a != mainThread->joinThreads.end(); ++a) {
            if((*a)->tid == tid) {
                joinThread = *a;
                mainThread->joinThreads.erase(a);
                break;
            }
        }
        mainThread->joinThreadsLock.Unlock();

        if(joinThread == nullptr)
            return ErrorThreadNotFound;

        if(ThreadBlock(ThreadState::JOIN, (uint64)joinThread) != OK) {
            mainThread->joinThreadsLock.Spinlock();
            mainThread->joinThreads.push_back(joinThread);
            mainThread->joinThreadsLock.Unlock();

            ThreadCheckFlags();

            return ErrorInterrupted;
        }

        exitCode = joinThread->exitCode;
        delete[] (char*)(joinThread->kernelStack - KernelStackSize);
        delete joinThread;

        return OK;
    }

    void Tick(IDT::Registers* regs) {
        auto& cpuData = g_CPUData.Get();

        cpuData.activeListLock.Spinlock_Raw();
        auto next = FindNextThread();
        cpuData.activeListLock.Unlock_Raw();

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

    int64 ThreadSleep(uint64 ms) {
        return ThreadBlock(ThreadState::WAIT, ms);
    }

    void ThreadExit(uint64 code) {
        auto& cpuData = g_CPUData.Get();
        auto tInfo = cpuData.currentThread;

        tInfo->joinThreadsLock.Spinlock();
        g_InitThread->joinThreadsLock.Spinlock();
        for(auto jt : tInfo->joinThreads) {
            jt->mainThread = jt;
            jt->killPending = true;
            g_InitThread->joinThreads.push_back(jt);
        }
        g_InitThread->joinThreadsLock.Unlock();
        tInfo->joinThreadsLock.Unlock();

        ThreadSetSticky();

        if(tInfo->fds->refCount.DecAndCheckZero()) {
            for(const auto& fd : tInfo->fds->fds) {
                if(fd.sysDesc != 0)
                    VFS::Close(fd.sysDesc);
            }
            delete tInfo->fds;
        }

        if(tInfo->memSpace->refCount.DecAndCheckZero()) {
            if(tInfo->memSpace->pml4Entry != 0)
                MemoryManager::FreeProcessMap(tInfo->memSpace->pml4Entry);
            delete tInfo->memSpace;
        }

        tInfo->exitCode = code;
        tInfo->state.type = ThreadState::FINISHED;

        cpuData.activeListLock.Spinlock_Cli();
        cpuData.activeList.erase(tInfo);
        cpuData.activeListLock.Unlock_Cli();

        ThreadYield();
    }

    int64 ThreadGetTID() {
        auto tInfo = g_CPUData.Get().currentThread;
        return tInfo->tid;
    }

    uint64 ThreadGetUID() {
        auto tInfo = g_CPUData.Get().currentThread;

        if(tInfo->user != nullptr)
            return tInfo->user->uid;
        else
            return 0;
    }
    uint64 ThreadGetGID() {
        auto tInfo = g_CPUData.Get().currentThread;

        if(tInfo->user != nullptr)
            return tInfo->user->gid;
        else
            return 0;
    }
    const char* ThreadGetUserName() {
        auto tInfo = g_CPUData.Get().currentThread;

        if(tInfo->user != nullptr)
            return tInfo->user->name;
        else
            return "--Kernel--";
    }

    static ThreadFileDescriptor* FindFreeFD(ThreadInfo* tInfo) {
        int64 lastFD = -1;
        for(auto a = tInfo->fds->fds.begin(); a != tInfo->fds->fds.end(); ++a) {
            if(a->id != lastFD + 1) {
                auto res = tInfo->fds->fds.insert(a, { lastFD + 1, 0 });
                return &*res;
            } else {
                lastFD = a->id;
            }
        }

        tInfo->fds->fds.push_back({ lastFD + 1, 0 });
        return &tInfo->fds->fds.back();
    }

    int64 ThreadAddFileDescriptor(uint64 sysDesc) {
        auto tInfo = g_CPUData.Get().currentThread;

        tInfo->fds->lock.Spinlock();
        auto fd = FindFreeFD(tInfo);
        fd->sysDesc = sysDesc;
        tInfo->fds->lock.Unlock();

        return fd->id;
    }

    int64 ThreadReplaceFileDescriptor(int64 oldPDesc, int64 newPDesc) {
        if(oldPDesc == newPDesc)
            return OK;
        
        auto tInfo = g_CPUData.Get().currentThread;

        ThreadFileDescriptor* oldFD = nullptr;
        ThreadFileDescriptor* newFD = nullptr;

        tInfo->fds->lock.Spinlock();
        for(auto& fd : tInfo->fds->fds) {
            if(fd.id == oldPDesc)
                oldFD = &fd;
            if(fd.id == newPDesc)
                newFD = &fd;
        }
        tInfo->fds->lock.Unlock();

        if(newFD == nullptr || newFD->sysDesc == 0)
            return ErrorInvalidFD;

        if(oldFD == nullptr) {
            auto a = tInfo->fds->fds.begin();

            tInfo->fds->lock.Spinlock();
            for(; a != tInfo->fds->fds.end(); ++a) {
                if(a->id > oldPDesc)
                    break;
            }

            auto n = tInfo->fds->fds.insert(a, { oldPDesc, 0 });
            oldFD = &*n;
            tInfo->fds->lock.Unlock();
        }

        if(oldFD->sysDesc == 0) {
            oldFD->sysDesc = newFD->sysDesc;
            VFS::AddRef(oldFD->sysDesc);
            return OK;
        } else if(oldFD->sysDesc == newFD->sysDesc) {
            return OK;
        } else {
            VFS::Close(oldFD->sysDesc);
            VFS::AddRef(newFD->sysDesc);
            oldFD->sysDesc = newFD->sysDesc;
            return OK;
        }
    }

    int64 ThreadCloseFileDescriptor(int64 desc) {
        auto tInfo = g_CPUData.Get().currentThread;

        tInfo->fds->lock.Spinlock();
        for(auto a = tInfo->fds->fds.begin(); a != tInfo->fds->fds.end(); ++a) {
            if(a->id == desc) {
                if(a->sysDesc == 0) {
                    tInfo->fds->lock.Unlock();
                    return ErrorInvalidFD;
                } else {
                    VFS::Close(a->sysDesc);
                    a->sysDesc = 0;
                    tInfo->fds->lock.Unlock();
                    return OK;
                }
            }
        }
        tInfo->fds->lock.Unlock();
        return ErrorInvalidFD;
    }

    int64 ThreadGetSystemFileDescriptor(int64 pDesc, uint64& sysDesc) {
        auto tInfo = g_CPUData.Get().currentThread;

        tInfo->fds->lock.Spinlock();
        for(auto a = tInfo->fds->fds.begin(); a != tInfo->fds->fds.end(); ++a) {
            if(a->id == pDesc) {
                if(a->sysDesc == 0) {
                    tInfo->fds->lock.Unlock();
                    return ErrorInvalidFD;
                } else {
                    sysDesc = a->sysDesc;
                    tInfo->fds->lock.Unlock();
                    return OK;
                }
            }
        }
        tInfo->fds->lock.Unlock();
        return ErrorInvalidFD;
    }

    void ThreadSetFS(uint64 val) {
        g_CPUData.Get().currentThread->userFSBase = val;
    }
    void ThreadSetGS(uint64 val) {
        g_CPUData.Get().currentThread->userGSBase = val;
    }

    void ThreadExec(uint64 pml4Entry, IDT::Registers* regs) {
        auto tInfo = g_CPUData.Get().currentThread;

        ThreadSetSticky();

        if(tInfo->memSpace->refCount.DecAndCheckZero()) {
            tInfo->memSpace->refCount = 1;
            if(tInfo->memSpace->pml4Entry != 0)
                MemoryManager::FreeProcessMap(tInfo->memSpace->pml4Entry);
            tInfo->memSpace->pml4Entry = pml4Entry;
        } else {
            tInfo->memSpace = new ThreadMemSpace();
            tInfo->memSpace->refCount = 1;
            tInfo->memSpace->pml4Entry = pml4Entry;
        }

        MemoryManager::SwitchProcessMap(tInfo->memSpace->pml4Entry);

        ThreadDisableInterrupts();
        tInfo->cliCount = 0;
        tInfo->stickyCount = 0;

        tInfo->registers = *regs;
        tInfo->registers.rflags = CPU::FLAGS_IF;
        GoToThread(&tInfo->registers);
    }

    void ThreadSetSticky() {
        g_CPUData.Get().currentThread->stickyCount++;
    }

    void ThreadUnsetSticky() {
        g_CPUData.Get().currentThread->stickyCount--;
    }

    void ThreadDisableInterrupts() {
        g_CPUData.Get().currentThread->cliCount++;
        IDT::DisableInterrupts();
    }
    void ThreadEnableInterrupts() {
        if(g_CPUData.Get().currentThread->cliCount-- == 1)
            IDT::EnableInterrupts();
    }

    void ThreadCheckFlags() {
        if(g_CPUData.Get().currentThread->killPending)
            ThreadExit(1);
    }

    extern "C" void ThreadSetPageFaultRip(uint64 rip) {
        g_CPUData.Get().currentThread->faultRip = rip;
    }

    static void ThreadFaultHandler(const char* msg) {
        klog_error("Fault", "Thread %i exiting due to error %s", ThreadGetTID(), msg);
        ThreadExit(1);
    }

    void ThreadSetupPageFaultHandler(IDT::Registers* regs, const char* msg) {
        auto tInfo = g_CPUData.Get().currentThread;

        if(tInfo->faultRip != 0) {
            regs->rip = tInfo->faultRip;
        } else {
            regs->rip = (uint64)&ThreadFaultHandler;
            regs->userrsp = tInfo->kernelStack;
            regs->rdi = (uint64)msg;
            regs->cs = GDT::KernelCode;
            regs->ds = GDT::KernelData;
            regs->ss = GDT::KernelData;
            regs->rflags = CPU::FLAGS_IF;
            tInfo->cliCount = 0;
            tInfo->stickyCount = 0;
        }
    }

    ThreadInfo* GetCurrentThreadInfo() {
        return g_CPUData.Get().currentThread;
    }

}