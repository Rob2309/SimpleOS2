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
#include "klib/string.h"

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
        ktl::AnchorList<ThreadInfo, &ThreadInfo::activeListAnchor> activeList;

        uint64 lastTSC;
    };

    static DECLARE_PER_CPU(CPUData, g_CPUData);
    static uint64 g_ThreadStructSize;
    static Atomic<uint64> g_TIDCounter = 1;

    static StickyLock g_GlobalThreadListLock;
    static ktl::AnchorList<ThreadInfo, &ThreadInfo::globalListAnchor> g_GlobalThreadList;

    static ThreadInfo* g_InitThread = nullptr;

    static void TimerEvent(IDT::Registers* regs) {
        Tick(regs);
    }

    static void UpdateEvents() {
        auto& cpuData = g_CPUData.Get();

        auto tsc = Time::GetTSC();
        auto dur = tsc - cpuData.lastTSC;
        cpuData.lastTSC = tsc;

        for(auto a = cpuData.activeList.begin(); a != cpuData.activeList.end(); ) {
            auto& tInfo = *a;

            if(tInfo.state.type == ThreadState::EXITED) {
                ++a;
                continue;
            } else if(tInfo.state.type == ThreadState::FINISHED) {
                cpuData.activeList.erase(a++);
                tInfo.state.type = ThreadState::EXITED;
                klog_info_isr("Scheduler", "Thread %i has exited with code %i", tInfo.tid, tInfo.exitCode);
            } else {
                if(tInfo.state.type == ThreadState::READY) {

                } else if(tInfo.state.type == ThreadState::QUEUE_LOCK) {
                    
                } else if(tInfo.killPending) {
                    tInfo.state.type = ThreadState::READY;
                    tInfo.registers.rax = ErrorInterrupted;
                } else if(tInfo.state.type == ThreadState::SLEEP) {
                    if(tInfo.state.arg <= dur) {
                        tInfo.state.type = ThreadState::READY;
                        tInfo.registers.rax = OK;
                    } else {
                        tInfo.state.arg -= dur;
                    }
                } else if(tInfo.state.type == ThreadState::JOIN) {
                    auto jt = (ThreadInfo*)tInfo.state.arg;
                    if(jt->state.type == ThreadState::EXITED) {
                        tInfo.state.type = ThreadState::READY;
                        tInfo.registers.rax = 0;
                    }
                }

                ++a;
            }
        }
    }

    static ThreadInfo* FindNextThread() {
        auto& cpuData = g_CPUData.Get();

        for(auto a = cpuData.activeList.begin(); a != cpuData.activeList.end(); ++a) {
            auto& tInfo = *a;

            if(tInfo.state.type == ThreadState::READY) {
                cpuData.activeList.erase(a);
                cpuData.activeList.push_back(&tInfo);
                return &tInfo;
            }
        }

        return &cpuData.idleThread;
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
        auto& cpuData = g_CPUData.Get();

        cpuData.idleThreadMemSpace.refCount = 1;
        cpuData.idleThreadMemSpace.pml4Entry = 0;
        
        cpuData.idleThreadFDs.refCount = 1;
        
        cpuData.idleThread.mainThread = &cpuData.idleThread;
        cpuData.idleThread.tid = 0;
        cpuData.idleThread.killPending = false;
        cpuData.idleThread.memSpace = &cpuData.idleThreadMemSpace;
        cpuData.idleThread.fds = &cpuData.idleThreadFDs;
        cpuData.idleThread.uid = 0;
        cpuData.idleThread.gid = 0;
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
        tInfo->uid = 0;
        tInfo->gid = 0;
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

        g_GlobalThreadListLock.Spinlock_Cli();
        g_GlobalThreadList.push_back(tInfo);
        g_GlobalThreadListLock.Unlock_Cli();

        return tInfo;
    }

    void CreateInitKernelThread(int64 (*func)(uint64, uint64)) {
        APIC::SetTimerEvent(TimerEvent);

        auto tInfo = _CreateKernelThread(func);
        g_InitThread = tInfo;
    }

    int64 CreateKernelThread(int64 (*func)(uint64, uint64), uint64 arg1, uint64 arg2) {
        auto tInfo = _CreateKernelThread(func, arg1, arg2);

        g_InitThread->childThreadsLock.Spinlock();
        g_InitThread->childThreads.push_back(tInfo);
        g_InitThread->childThreadsLock.Unlock();

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
                    fds->fds.push_back( fd );
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
        newT->uid = tInfo->uid;
        newT->gid = tInfo->gid;
        newT->state.type = ThreadState::READY;
        newT->kernelStack = (uint64)new char[KernelStackSize] + KernelStackSize;
        newT->stickyCount = 0;
        newT->cliCount = 0;
        newT->faultRip = 0;
        newT->registers = *regs;
        newT->registers.rflags |= CPU::FLAGS_IF;
        newT->fpuBuffer = new char[SSE::GetFPUBlockSize()];
        kmemset(newT->fpuBuffer, 0, SSE::GetFPUBlockSize());
        SSE::InitFPUBlock(newT->fpuBuffer);

        tInfo->mainThread->childThreadsLock.Spinlock();
        tInfo->mainThread->childThreads.push_back(newT);
        tInfo->mainThread->childThreadsLock.Unlock();

        cpuData.activeListLock.Spinlock_Cli();
        cpuData.activeList.push_back(newT);
        cpuData.activeListLock.Unlock_Cli();

        g_GlobalThreadListLock.Spinlock_Cli();
        g_GlobalThreadList.push_back(newT);
        g_GlobalThreadListLock.Unlock_Cli();

        return newT->tid;
    }
    SYSCALL_DEFINE3(syscall_thread_create, uint64 entry, uint64 stack, uint64 arg) {
        IDT::Registers regs;
        regs.rip = entry;
        regs.userrsp = stack;
        regs.rdi = arg;
        regs.cs = GDT::UserCode;
        regs.ds = GDT::UserData;
        regs.ss = GDT::UserData;

        int64 res = CloneThread(true, true, true, &regs);
        return res;
    }

    int64 ThreadBlock(ThreadState::Type type, uint64 arg) {
        auto& cpuData = g_CPUData.Get();
        auto tInfo = cpuData.currentThread;
        
        ThreadSetSticky();

        tInfo->state.arg = arg;
        tInfo->state.type = type;

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
        auto tInfo = g_CPUData.Get().currentThread;
        auto mainThread = tInfo->mainThread;

        ThreadInfo* detachThread = nullptr;

        mainThread->childThreadsLock.Spinlock();
        for(auto a = mainThread->childThreads.begin(); a != mainThread->childThreads.end(); ++a) {
            if(a->tid == tid) {
                if(a->mainThread != &*a) {
                    mainThread->childThreadsLock.Unlock();
                    return ErrorDetachSubThread;
                }
                detachThread = &*a;
                mainThread->childThreads.erase(a);
                break;
            }
        }
        mainThread->childThreadsLock.Unlock();

        if(detachThread == nullptr)
            return ErrorThreadNotFound;

        g_InitThread->childThreadsLock.Spinlock();
        g_InitThread->childThreads.push_back(detachThread);
        g_InitThread->childThreadsLock.Unlock();

        return OK;
    }
    SYSCALL_DEFINE1(syscall_detach, int64 tid) {
        return ThreadDetach(tid);
    }

    int64 ThreadJoin(int64 tid, int64& exitCode) {
        auto mainThread = g_CPUData.Get().currentThread->mainThread;

        ThreadInfo* joinThread = nullptr;
        mainThread->childThreadsLock.Spinlock();
        for(auto a = mainThread->childThreads.begin(); a != mainThread->childThreads.end(); ++a) {
            if(a->tid == tid) {
                joinThread = &*a;
                mainThread->childThreads.erase(a);
                break;
            }
        }
        mainThread->childThreadsLock.Unlock();

        if(joinThread == nullptr)
            return ErrorThreadNotFound;

        int64 error = ThreadBlock(ThreadState::JOIN, (uint64)joinThread);
        if(error != OK) {
            mainThread->childThreadsLock.Spinlock();
            mainThread->childThreads.push_back(joinThread);
            mainThread->childThreadsLock.Unlock();

            return error;
        }

        g_GlobalThreadListLock.Spinlock_Cli();
        g_GlobalThreadList.erase(joinThread);
        g_GlobalThreadListLock.Unlock_Cli();

        exitCode = joinThread->exitCode;
        delete[] (char*)(joinThread->kernelStack - KernelStackSize);
        delete joinThread;

        return OK;
    }
    SYSCALL_DEFINE1(syscall_join, int64 tid) {
        int64 exitCode;
        int64 err = ThreadJoin(tid, exitCode);
        if(err != OK)
            return err;
        return exitCode;
    }
    int64 ThreadTryJoin(int64 tid, int64& exitCode) {
        auto mainThread = g_CPUData.Get().currentThread->mainThread;

        ThreadInfo* joinThread = nullptr;
        mainThread->childThreadsLock.Spinlock();
        for(auto a = mainThread->childThreads.begin(); a != mainThread->childThreads.end(); ++a) {
            if(a->tid == tid) {
                if(a->state.type == ThreadState::EXITED) {
                    joinThread = &*a;

                    exitCode = a->exitCode;
                    mainThread->childThreads.erase(a);
                    mainThread->childThreadsLock.Unlock();

                    g_GlobalThreadListLock.Spinlock_Cli();
                    g_GlobalThreadList.erase(joinThread);
                    g_GlobalThreadListLock.Unlock_Cli();

                    delete[] (char*)(joinThread->kernelStack - KernelStackSize);
                    delete joinThread;

                    return OK;
                } else {
                    mainThread->childThreadsLock.Unlock();
                    return ErrorThreadNotExited;
                }
            }
        }
        mainThread->childThreadsLock.Unlock();
        return ErrorThreadNotFound;
    }
    SYSCALL_DEFINE1(syscall_try_join, int64 tid) {
        int64 exitCode;
        int64 err = ThreadTryJoin(tid, exitCode);
        if(err != OK)
            return err;
        return exitCode;
    }

    int64 ThreadKill(int64 tid) {
        auto tInfo = g_CPUData.Get().currentThread;
        auto uid = tInfo->uid;

        // Thread suicide
        if(tid == tInfo->tid)
            ThreadExit(1);

        ThreadInfo* killThread = nullptr;
        g_GlobalThreadListLock.Spinlock();
        for(auto& tInfo : g_GlobalThreadList) {
            if(tInfo.tid == tid) {
                killThread = &tInfo;
                break;
            }
        }

        if(killThread == nullptr) {
            g_GlobalThreadListLock.Unlock();
            return ErrorThreadNotFound;
        }
        if(uid != 0 && uid != killThread->uid) {
            g_GlobalThreadListLock.Unlock();
            return ErrorPermissionDenied;
        }

        killThread->killPending = true;
        g_GlobalThreadListLock.Unlock();
        return OK;
    }
    SYSCALL_DEFINE1(syscall_kill, int64 tid) {
        return ThreadKill(tid);
    }

    void Tick(IDT::Registers* regs) {
        auto& cpuData = g_CPUData.Get();

        if(cpuData.currentThread->stickyCount != 0)
            return;

        cpuData.activeListLock.Spinlock_Raw();
        UpdateEvents();
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
        return ThreadBlock(ThreadState::SLEEP, ms * Time::GetTSCTicksPerMilli());
    }
    SYSCALL_DEFINE1(syscall_wait, uint64 ms) {
        return ThreadSleep(ms);
    }

    static void KillChildThreads(ThreadInfo* tInfo) {
        while(true) {
            ThreadInfo* jt = nullptr;

            tInfo->childThreadsLock.Spinlock();
            if(!tInfo->childThreads.empty()) {
                jt = &tInfo->childThreads.back();
                tInfo->childThreads.pop_back();
            }
            tInfo->childThreadsLock.Unlock();

            if(jt == nullptr)
                break;

            jt->killPending = true;
            
            if(ThreadBlock(ThreadState::JOIN, (uint64)jt) != OK) {
                tInfo->childThreadsLock.Spinlock();
                tInfo->childThreads.push_back(jt);
                tInfo->childThreadsLock.Unlock();
            }
        }
    }

    void ThreadExit(uint64 code) {
        auto& cpuData = g_CPUData.Get();
        auto tInfo = cpuData.currentThread;

        KillChildThreads(tInfo);

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

        ThreadYield();
    }
    SYSCALL_DEFINE1(syscall_exit, uint64 code) {
        ThreadExit(code);
        return 1;
    }

    int64 ThreadGetTID() {
        auto tInfo = g_CPUData.Get().currentThread;
        return tInfo->tid;
    }
    SYSCALL_DEFINE0(syscall_gettid) {
        return ThreadGetTID();
    }

    uint64 ThreadGetUID() {
        auto tInfo = g_CPUData.Get().currentThread;

        return tInfo->uid;
    }
    uint64 ThreadGetGID() {
        auto tInfo = g_CPUData.Get().currentThread;

        return tInfo->gid;
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
    SYSCALL_DEFINE2(syscall_copyfd, int64 oldFD, int64 newFD) {
        return ThreadReplaceFileDescriptor(oldFD, newFD);
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
    SYSCALL_DEFINE1(syscall_setfs, uint64 val) {
        ThreadSetFS(val);
        MSR::Write(MSR::RegFSBase, val);
        return OK;
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

        LoadThreadState(tInfo);
        GoToThread(&tInfo->registers);
    }

    void ThreadSetSticky() {
        auto tInfo = g_CPUData.Get().currentThread;
        tInfo->stickyCount++;
    }

    void ThreadUnsetSticky() {
        auto tInfo = g_CPUData.Get().currentThread;
        tInfo->stickyCount--;
    }

    void ThreadDisableInterrupts() {
        g_CPUData.Get().currentThread->cliCount++;
        IDT::DisableInterrupts();
    }
    void ThreadEnableInterrupts() {
        g_CPUData.Get().currentThread->cliCount--;
        if(g_CPUData.Get().currentThread->cliCount == 0)
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

    void ThreadSetupFaultHandler(IDT::Registers* regs, const char* msg) {
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