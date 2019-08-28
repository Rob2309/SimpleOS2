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
#include "arch/SSE.h"
#include "ktl/new.h"
#include "syscalls/SyscallDefine.h"
#include "time/Time.h"

namespace Scheduler {

    extern "C" void IdleThread();

    static uint64 g_PIDCounter = 1;
    static uint64 g_TIDCounter = 1;

    static ThreadInfo* FindNextThread();
    static ThreadInfo* FindNextThreadFromInterrupt();

    extern "C" void ReturnToThread(IDT::Registers* regs);
    extern "C" void ContextSwitchAndReturn(IDT::Registers* from, IDT::Registers* to);

    struct CPUData {
        uint64 currentThreadKernelStack;

        ThreadInfo* idleThread;
        ThreadInfo* currentThread;

        StickyLock threadListLock;
        ktl::nlist<ThreadInfo> threadList;
        ktl::nlist<ThreadInfo> deadThreads;

        uint64 lastTSC;
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

    static uint64 g_ThreadStructSize;

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
        if(!inKernelMode)
            MSR::Write(MSR::RegFSBase, g_CPUData[coreID].currentThread->userFSBase);
        SSE::RestoreFPUBlock(thread->fpuState);
    }

    static void FreeProcess(ProcessInfo* pInfo)
    {
        for(const ProcessFileDescriptor& fd : pInfo->fileDescs) {
            VFS::Close(fd.desc);
        }

        MemoryManager::FreeProcessMap(pInfo->pml4Entry);
        delete pInfo;
    }

    static void SetupKillHandler(ThreadInfo* tInfo, uint64 exitCode);

    static void KillEvent(IDT::Registers* regs) {
        APIC::SignalEOI();

        uint64 coreID = SMP::GetLogicalCoreID();

        ProcessInfo* pInfo = g_KillProcess;

        g_CPUData[coreID].threadListLock.Spinlock_Raw();
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
        g_CPUData[coreID].threadListLock.Unlock_Raw();

        __asm__ __volatile__ (
            "lock incq (%0)"
            : : "r"(&g_KillCount)
        );

        SetContext(g_CPUData[coreID].currentThread, regs);
    }

    static ThreadInfo* CreateThreadStruct()
    {
        uint64 coreID = SMP::GetLogicalCoreID();

        g_CPUData[coreID].threadListLock.Spinlock_Cli();
        if(!g_CPUData[coreID].deadThreads.empty()) {
            ThreadInfo* res = g_CPUData[coreID].deadThreads.back();
            g_CPUData[coreID].deadThreads.pop_back();
            g_CPUData[coreID].threadListLock.Unlock_Cli();

            uint64 kernelStack = res->kernelStack;
            kmemset(res, 0, g_ThreadStructSize);
            res->kernelStack = kernelStack;
            SSE::InitFPUBlock(res->fpuState);

            return res;
        }
        g_CPUData[coreID].threadListLock.Unlock_Cli();
        
        ThreadInfo* n = (ThreadInfo*)new char[g_ThreadStructSize];
        kmemset(n, 0, g_ThreadStructSize);
        SSE::InitFPUBlock(n->fpuState);
        n->kernelStack = (uint64)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages(KernelStackPages)) + KernelStackSize;
        return n;
    }

    uint64 CreateInitThread(void (*func)()) {
        uint64 coreID = SMP::GetLogicalCoreID();

        ThreadInfo* tInfo = (ThreadInfo*)MemoryManager::PhysToKernelPtr(MemoryManager::EarlyAllocatePages(NUM_PAGES(g_ThreadStructSize)));
        kmemset(tInfo, 0, g_ThreadStructSize);
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

    uint64 CreateUserThread(uint64 pml4Entry, IDT::Registers* regs, User* owner)
    {
        uint64 coreID = SMP::GetLogicalCoreID();

        ProcessInfo* pInfo = new ProcessInfo();
        pInfo->pid = g_PIDCounter++;
        pInfo->pml4Entry = pml4Entry;
        pInfo->owner = owner;

        ThreadInfo* tInfo = CreateThreadStruct();
        tInfo->tid = g_TIDCounter++;
        tInfo->process = pInfo;
        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        tInfo->userGSBase = 0;
        tInfo->registers = *regs;
        tInfo->unkillable = false;
        tInfo->killPending = false;

        pInfo->numThreads = 1;
        
        g_CPUData[coreID].threadListLock.Spinlock_Cli();
        g_CPUData[coreID].threadList.push_back(tInfo);
        uint64 ret = tInfo->tid;
        g_CPUData[coreID].threadListLock.Unlock_Cli();

        return ret;
    }

    uint64 CreateKernelThread(uint64 rip, uint64 arg1, uint64 arg2) {
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
        regs.rdi = arg1;
        regs.rsi = arg2;

        tInfo->tid = g_TIDCounter++;
        tInfo->process = nullptr;
        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        tInfo->userGSBase = 0;
        tInfo->registers = regs;
        tInfo->unkillable = true;
        tInfo->killPending = false;
        
        g_CPUData[coreID].threadListLock.Spinlock_Cli();
        g_CPUData[coreID].threadList.push_back(tInfo);
        uint64 ret = tInfo->tid;
        g_CPUData[coreID].threadListLock.Unlock_Cli();

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
        pInfo->owner = oldPInfo->owner;
        
        oldPInfo->fileDescLock.Spinlock();
        for(const ProcessFileDescriptor& fd : oldPInfo->fileDescs) {
            ProcessFileDescriptor newFD;
            newFD.desc = fd.desc;
            newFD.id = fd.id;
            pInfo->fileDescs.push_back(newFD);
            VFS::AddRef(newFD.desc);
        }
        oldPInfo->fileDescLock.Unlock();

        ThreadInfo* tInfo = CreateThreadStruct();
        tInfo->tid = g_TIDCounter++;
        tInfo->process = pInfo;
        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        tInfo->userGSBase = 0;
        tInfo->registers = *regs;
        kmemcpy(tInfo->fpuState, g_CPUData[coreID].currentThread->fpuState, SSE::GetFPUBlockSize());

        pInfo->numThreads = 1;
        
        g_CPUData[coreID].threadListLock.Spinlock_Cli();
        g_CPUData[coreID].threadList.push_back(tInfo);
        uint64 ret = tInfo->tid;
        g_CPUData[coreID].threadListLock.Unlock_Cli();

        return ret;
    }

    static void UpdateEvents() {
        uint64 coreID = SMP::GetLogicalCoreID();

        uint64 tsc = Time::GetTSC();
        uint64 passed = tsc - g_CPUData[coreID].lastTSC;
        g_CPUData[coreID].lastTSC = tsc;

        ktl::nlist<ThreadInfo> transferList;

        g_CPUData[coreID].threadListLock.Spinlock_Raw();
        for(auto a = g_CPUData[coreID].threadList.begin(); a != g_CPUData[coreID].threadList.end(); ) {
            ThreadInfo* p = *a;
            switch(p->blockEvent.type) {
            case ThreadBlockEvent::TYPE_WAIT:
                if(p->blockEvent.wait.remainingTicks <= passed) {
                    p->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
                } else {
                    p->blockEvent.wait.remainingTicks -= passed;
                }
                ++a;
                break;
            case ThreadBlockEvent::TYPE_TRANSFER:
                g_CPUData[coreID].threadList.erase(a++);
                transferList.push_back(p);
                break;

            default:
                ++a;
                break;
            }
        }
        g_CPUData[coreID].threadListLock.Unlock_Raw();

        for(auto a = transferList.begin(); a != transferList.end(); ) {
            uint64 tCoreID = a->blockEvent.transfer.coreID;

            ThreadInfo* p = *a;
            p->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
            ++a;

            g_CPUData[tCoreID].threadListLock.Spinlock_Raw();
            g_CPUData[tCoreID].threadList.push_back(p);
            g_CPUData[tCoreID].threadListLock.Unlock_Raw();
        }
    }

    static ThreadInfo* FindNextThread() {
        uint64 coreID = SMP::GetLogicalCoreID();

        g_CPUData[coreID].threadListLock.Spinlock_Cli();
        for(auto a = g_CPUData[coreID].threadList.begin(); a != g_CPUData[coreID].threadList.end(); ++a) {
            ThreadInfo* p = *a;
            if(p->blockEvent.type == ThreadBlockEvent::TYPE_NONE) {
                g_CPUData[coreID].threadList.erase(a);
                g_CPUData[coreID].threadList.push_back(p);
                g_CPUData[coreID].threadListLock.Unlock_Cli();
                return p;
            }
        }

        g_CPUData[coreID].threadListLock.Unlock_Cli();
        return g_CPUData[coreID].idleThread;
    }

    static ThreadInfo* FindNextThreadFromInterrupt() {
        uint64 coreID = SMP::GetLogicalCoreID();

        g_CPUData[coreID].threadListLock.Spinlock_Raw();
        for(auto a = g_CPUData[coreID].threadList.begin(); a != g_CPUData[coreID].threadList.end(); ++a) {
            ThreadInfo* p = *a;
            if(p->blockEvent.type == ThreadBlockEvent::TYPE_NONE) {
                g_CPUData[coreID].threadList.erase(a);
                g_CPUData[coreID].threadList.push_back(p);
                g_CPUData[coreID].threadListLock.Unlock_Raw();
                return p;
            }
        }

        g_CPUData[coreID].threadListLock.Unlock_Raw();
        return g_CPUData[coreID].idleThread;
    }

    static void SaveThreadInfo(IDT::Registers* regs)
    {
        uint64 coreID = SMP::GetLogicalCoreID();
        g_CPUData[coreID].currentThread->registers = *regs;     // save all registers in process info
        SSE::SaveFPUBlock(g_CPUData[coreID].currentThread->fpuState);
    }

    void Tick(IDT::Registers* regs)
    {
        uint64 coreID = SMP::GetLogicalCoreID();
        if(g_CPUData[coreID].currentThread->stickyCount > 0)
            return;

        SaveThreadInfo(regs);

        // Find next process to execute
        UpdateEvents();
        ThreadInfo* nextProcess = FindNextThreadFromInterrupt();

        SetContext(nextProcess, regs);
    }

    void Init(uint64 numCores) {
        g_CPUData = (CPUData*)MemoryManager::PhysToKernelPtr(MemoryManager::EarlyAllocatePages(NUM_PAGES(numCores * sizeof(CPUData))));
        for(uint64 i = 0; i < numCores; i++)
            new(&g_CPUData[i]) CPUData();

        IDT::SetISR(ISRNumbers::IPIKillProcess, KillEvent);

        g_ThreadStructSize = sizeof(ThreadInfo) + SSE::GetFPUBlockSize();
    }

    void Start()
    {
        uint64 coreID = SMP::GetLogicalCoreID();

        IDT::DisableInterrupts();
        APIC::SetTimerEvent(TimerEvent);
        APIC::StartTimer(10);

        // Init idle process
        ThreadInfo* p = (ThreadInfo*)MemoryManager::PhysToKernelPtr(MemoryManager::EarlyAllocatePages(NUM_PAGES(g_ThreadStructSize)));
        kmemset(p, 0, g_ThreadStructSize);
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
        ThreadDisableInterrupts();
        ThreadInfo* nextThread = FindNextThread();
        if(nextThread == g_CPUData[coreID].currentThread) {
            ThreadEnableInterrupts();
            return;
        } else {
            IDT::Registers nextRegs;
            SSE::SaveFPUBlock(g_CPUData[coreID].currentThread->fpuState);
            SetContext(nextThread, &nextRegs);
            ContextSwitchAndReturn(myRegs, &nextRegs);
            ThreadEnableInterrupts();
        }
    }

    void ThreadYield()
    {
        Yield();
    }

    void ThreadWait(uint64 ms)
    {
        uint64 coreID = SMP::GetLogicalCoreID();

        ThreadSetSticky();

        g_CPUData[coreID].currentThread->blockEvent.type = ThreadBlockEvent::TYPE_WAIT;
        g_CPUData[coreID].currentThread->blockEvent.wait.remainingTicks = ms * Time::GetTSCTicksPerMilli();
        
        Yield();

        ThreadUnsetSticky();
    }
    SYSCALL_DEFINE1(syscall_wait, uint64 ms) {
        ThreadWait(ms);
        return 0;
    }

    void ThreadExit(uint64 code)
    {
        kprintf("%C[%i.%i %s]%C Exiting with code %i on Core %i\n", 200, 50, 50, Scheduler::ThreadGetPID(), Scheduler::ThreadGetTID(), Scheduler::ThreadGetUserName(), 255, 255, 255, code, SMP::GetLogicalCoreID());

        uint64 coreID = SMP::GetLogicalCoreID();

        ThreadInfo* tInfo = g_CPUData[coreID].currentThread;
        ProcessInfo* pInfo = tInfo->process;

        if(pInfo != nullptr) {
            pInfo->mainLock.Spinlock();
            pInfo->numThreads--;
            if(pInfo->numThreads == 0) {
                tInfo->process = nullptr;

                FreeProcess(pInfo);
            } else {
                pInfo->mainLock.Unlock();
            }
        }

        g_CPUData[coreID].threadListLock.Spinlock_Cli();
        g_CPUData[coreID].threadList.erase(tInfo);
        g_CPUData[coreID].deadThreads.push_back(tInfo);
        g_CPUData[coreID].threadListLock.Unlock_Cli();
        
        IDT::Registers regs;
        ThreadDisableInterrupts();
        ThreadInfo* next = FindNextThread();
        SetContext(next, &regs);
        ReturnToThread(&regs);
    }
    SYSCALL_DEFINE1(syscall_exit, uint64 code) {
        ThreadExit(code);
    }

    uint64 ThreadGetTID()
    {
        uint64 coreID = SMP::GetLogicalCoreID();
        return g_CPUData[coreID].currentThread->tid;
    }
    SYSCALL_DEFINE0(syscall_gettid) {
        return ThreadGetTID();
    }
    uint64 ThreadGetPID()
    {  
        uint64 coreID = SMP::GetLogicalCoreID();

        if(g_CPUData[coreID].currentThread->process != nullptr)
            return g_CPUData[coreID].currentThread->process->pid;
        else
            return 0;    
    }
    SYSCALL_DEFINE0(syscall_getpid) {
        return ThreadGetPID();
    }
    uint64 ThreadGetUID() {
        uint64 coreID = SMP::GetLogicalCoreID();

        if(g_CPUData[coreID].currentThread->process != nullptr)
            return g_CPUData[coreID].currentThread->process->owner->uid;
        else
            return 0; 
    }
    uint64 ThreadGetGID() {
        uint64 coreID = SMP::GetLogicalCoreID();

        if(g_CPUData[coreID].currentThread->process != nullptr)
            return g_CPUData[coreID].currentThread->process->owner->gid;
        else
            return 0; 
    }
    const char* ThreadGetUserName() {
        uint64 coreID = SMP::GetLogicalCoreID();

        if(g_CPUData[coreID].currentThread->process != nullptr)
            return g_CPUData[coreID].currentThread->process->owner->name;
        else
            return "-KernelThread-"; 
    }

    uint64 ThreadCreateThread(uint64 entry, uint64 stack, uint64 arg)
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
        tInfo->registers.rdi = arg;

        if(tInfo->process != nullptr) {
            tInfo->process->mainLock.Spinlock();
            tInfo->process->numThreads++;
            tInfo->process->mainLock.Unlock();
        }

        g_CPUData[coreID].threadListLock.Spinlock_Cli();
        g_CPUData[coreID].threadList.push_back(tInfo);
        uint64 res = tInfo->tid;
        g_CPUData[coreID].threadListLock.Unlock_Cli();

        return res;
    }
    SYSCALL_DEFINE3(syscall_thread_create, uint64 entry, uint64 stack, uint64 arg) {
        return ThreadCreateThread(entry, stack, arg);
    }

    static int64 FindLowestNotPresentFD(ProcessInfo* pInfo) {
        int64 lastDesc = -1;
        for(const auto& fd : pInfo->fileDescs) {
            if(fd.id != lastDesc + 1)
                break;
            lastDesc = fd.id;
        }
        
        return lastDesc + 1;
    }

    static void InsertFD(ProcessInfo* pInfo, const ProcessFileDescriptor& fd) {
        auto a = pInfo->fileDescs.begin();
        while(a != pInfo->fileDescs.end() && a->id < fd.id)
            ++a;
        
        pInfo->fileDescs.insert(a, fd);
    }

    static ProcessFileDescriptor* FindFD(ProcessInfo* pInfo, int64 id) {
        for(auto& fd : pInfo->fileDescs) {
            if(fd.id == id)
                return &fd;
            if(fd.id > id)
                break;
        }
        return nullptr;
    }

    int64 ProcessAddFileDescriptor(uint64 sysDescriptor) {
        uint64 coreID = SMP::GetLogicalCoreID();

        ProcessInfo* pInfo = g_CPUData[coreID].currentThread->process;
        pInfo->fileDescLock.Spinlock();
        for(ProcessFileDescriptor& d : pInfo->fileDescs) {
            if(d.desc == 0) {
                d.desc = sysDescriptor;
                pInfo->fileDescLock.Unlock();
                return d.id;
            }
        }

        ProcessFileDescriptor newDesc;
        newDesc.id = FindLowestNotPresentFD(pInfo);
        newDesc.desc = sysDescriptor;
        InsertFD(pInfo, newDesc);
        pInfo->fileDescLock.Unlock();

        return newDesc.id;
    };
    int64 ProcessReplaceFileDescriptor(int64 oldPDesc, int64 newPDesc) {
        if(oldPDesc == newPDesc) {
            return OK;
        }

        uint64 coreID = SMP::GetLogicalCoreID();

        ProcessInfo* pInfo = g_CPUData[coreID].currentThread->process;
        pInfo->fileDescLock.Spinlock();
        
        auto oldPFD = FindFD(pInfo, oldPDesc);
        if(oldPFD == nullptr || oldPFD->desc == 0) {
            pInfo->fileDescLock.Unlock();
            return ErrorInvalidFD;
        }

        auto newPFD = FindFD(pInfo, newPDesc);
        if(newPFD == nullptr) {
            ProcessFileDescriptor d;
            d.id = newPDesc;
            d.desc = oldPFD->desc;
            VFS::AddRef(d.desc);
            InsertFD(pInfo, d);
        } else if(newPFD->desc == 0) {
            newPFD->desc = oldPFD->desc;
            VFS::AddRef(newPFD->desc);
        } else if(newPFD->desc == oldPFD->desc) {

        } else {
            VFS::Close(newPFD->desc);
            newPFD->desc = oldPFD->desc;
            VFS::AddRef(newPFD->desc);
        }

        pInfo->fileDescLock.Unlock();
        return OK;
    }
    SYSCALL_DEFINE2(syscall_copyfd, int64 dest, int64 src) {
        return ProcessReplaceFileDescriptor(src, dest);
    }
    int64 ProcessCloseFileDescriptor(int64 descID) {
        uint64 coreID = SMP::GetLogicalCoreID();

        ProcessInfo* pInfo = g_CPUData[coreID].currentThread->process;

        pInfo->fileDescLock.Spinlock();
        auto fd = FindFD(pInfo, descID);
        if(fd == nullptr || fd->desc == 0) {
            pInfo->fileDescLock.Unlock();
            return ErrorInvalidFD;
        }
        
        int64 sysDesc = fd->desc;
        fd->desc = 0;
        pInfo->fileDescLock.Unlock();
        return VFS::Close(sysDesc);
    }
    int64 ProcessGetSystemFileDescriptor(int64 pDesc, uint64& sysDesc) {
        uint64 coreID = SMP::GetLogicalCoreID();

        ProcessInfo* pInfo = g_CPUData[coreID].currentThread->process;

        pInfo->fileDescLock.Spinlock();
        auto fd = FindFD(pInfo, pDesc);
        if(fd == nullptr || fd->desc == 0) {
            pInfo->fileDescLock.Unlock();
            return ErrorInvalidFD;
        }

        sysDesc = fd->desc;
        pInfo->fileDescLock.Unlock();
        return OK;
    }

    void ThreadSetFS(uint64 val) {
        uint64 coreID = SMP::GetLogicalCoreID();
        g_CPUData[coreID].currentThread->userFSBase = val;
    }
    SYSCALL_DEFINE1(syscall_setfs, uint64 fs) {
        ThreadSetFS(fs);
        MSR::Write(MSR::RegFSBase, fs);
        return 0;
    }

    void ThreadSetGS(uint64 val) {
        uint64 coreID = SMP::GetLogicalCoreID();
        g_CPUData[coreID].currentThread->userGSBase = val;
    }

    void ProcessExec(uint64 pml4Entry, IDT::Registers* regs)
    {
        uint64 coreID = SMP::GetLogicalCoreID();

        g_CPUData[coreID].currentThread->process->mainLock.Spinlock();
        uint64 oldPML4Entry = g_CPUData[coreID].currentThread->process->pml4Entry;
        g_CPUData[coreID].currentThread->process->pml4Entry = pml4Entry;
        g_CPUData[coreID].currentThread->process->mainLock.Unlock();

        MemoryManager::FreeProcessMap(oldPML4Entry);

        ThreadDisableInterrupts();
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
        tInfo->cliCount = 1;
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
        uint64 coreID = SMP::GetLogicalCoreID();
        ThreadInfo* tInfo = g_CPUData[coreID].currentThread;

        tInfo->stickyCount = 0;
        tInfo->cliCount = 0;
        regs->cs = GDT::KernelCode;
        regs->ds = GDT::KernelData;
        regs->ss = GDT::KernelData;
        regs->rip = (uint64)&ThreadKillProcess;
        regs->rflags = CPU::FLAGS_IF;
        regs->userrsp = tInfo->kernelStack;
        regs->rdi = (uint64)reason;
    }
    void ThreadKillProcess(const char* reason) {
        klog_error("Scheduler", "Killing process %i (owner=%s) because Thread %i requested it (%s)", ThreadGetPID(), ThreadGetUserName(), ThreadGetTID(), reason);

        uint64 coreID = SMP::GetLogicalCoreID();

        ThreadInfo* tInfo = g_CPUData[coreID].currentThread;
        ProcessInfo* pInfo = tInfo->process;

        if(pInfo != nullptr) {
            g_CPUData[coreID].threadListLock.Spinlock_Cli();
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
            g_CPUData[coreID].threadListLock.Unlock_Cli();

            g_KillLock.Spinlock();
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
    }
    void ThreadUnsetSticky() {
        uint64 coreID = SMP::GetLogicalCoreID();

        g_CPUData[coreID].currentThread->stickyCount--;
    }

    void ThreadDisableInterrupts() {
        uint64 coreID = SMP::GetLogicalCoreID();
        g_CPUData[coreID].currentThread->cliCount++;

        if(g_CPUData[coreID].currentThread->cliCount == 1)
            IDT::DisableInterrupts();
    }
    void ThreadEnableInterrupts() {
        uint64 coreID = SMP::GetLogicalCoreID();
        g_CPUData[coreID].currentThread->cliCount--;

        if(g_CPUData[coreID].currentThread->cliCount == 0)
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

    void ThreadMoveToCPU(uint64 logicalCoreID) {
        ThreadSetSticky();

        uint64 coreID = SMP::GetLogicalCoreID();

        ThreadInfo* tInfo = g_CPUData[coreID].currentThread;
        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_TRANSFER;
        tInfo->blockEvent.transfer.coreID = logicalCoreID;

        ThreadYield();
        ThreadUnsetSticky();
    }
    SYSCALL_DEFINE1(syscall_move_core, uint64 core) {
        ThreadMoveToCPU(core);
    }

    ThreadInfo* GetCurrentThreadInfo() {
        uint64 coreID = SMP::GetLogicalCoreID();
        return g_CPUData[coreID].currentThread;
    }

}