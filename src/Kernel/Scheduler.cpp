#include "Scheduler.h"

#include "Process.h"
#include "list.h"
#include "memutil.h"
#include "IDT.h"
#include "GDT.h"
#include "MemoryManager.h"
#include "MSR.h"
#include "APIC.h"
#include "conio.h"

namespace Scheduler {

    extern "C" void IdleThread();

    static std::nlist<ThreadInfo> g_ThreadList;

    static uint64 g_PIDCounter = 1;
    static uint64 g_TIDCounter = 1;

    static ThreadInfo* FindNextThread();

    extern "C" void ReturnToThread(IDT::Registers* regs);
    extern "C" void SyscallContextSwitch(IDT::Registers* from, IDT::Registers* to);

    static struct {
        uint64 currentThreadKernelStack;

        ThreadInfo* idleThread;
        ThreadInfo* currentThread;
    } g_CPUData;

    static void TimerEvent(IDT::Registers* regs)
    {
        Scheduler::Tick(regs);
    }

    uint64 CreateProcess(uint64 pml4Entry, IDT::Registers* regs)
    {
        ProcessInfo* pInfo = new ProcessInfo();
        memset(pInfo, 0, sizeof(ProcessInfo));
        pInfo->pid = g_PIDCounter++;
        pInfo->pml4Entry = pml4Entry;

        ThreadInfo* tInfo = new ThreadInfo();
        memset(tInfo, 0, sizeof(ThreadInfo));

        tInfo->tid = g_TIDCounter++;
        tInfo->process = pInfo;
        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        tInfo->kernelStack = (uint64)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages(KernelStackPages)) + KernelStackSize;
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
        uint64 kernelStack = (uint64)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages(KernelStackPages)) + KernelStackSize;

        IDT::Registers regs;
        memset(&regs, 0, sizeof(IDT::Registers));
        regs.cs = GDT::KernelCode;
        regs.ds = GDT::KernelData;
        regs.ss = GDT::KernelData;
        regs.rflags = 0b000000000001000000000;
        regs.rip = rip;
        regs.userrsp = kernelStack;

        ThreadInfo* tInfo = new ThreadInfo();
        memset(tInfo, 0, sizeof(ThreadInfo));

        tInfo->tid = g_TIDCounter++;
        tInfo->process = nullptr;
        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        tInfo->kernelStack = kernelStack;
        tInfo->userGSBase = 0;
        tInfo->registers = regs;
        
        IDT::DisableInterrupts();
        g_ThreadList.push_back(tInfo);
        uint64 ret = tInfo->tid;
        IDT::EnableInterrupts();

        return ret;
    }

    static void UpdateEvents() {
        for(auto& p : g_ThreadList) {
            switch(p.blockEvent.type) {
            case ThreadBlockEvent::TYPE_WAIT:
                if(p.blockEvent.wait.remainingMillis <= 10) {
                    p.blockEvent.type = ThreadBlockEvent::TYPE_NONE;
                } else {
                    p.blockEvent.wait.remainingMillis -= 10;
                }
                break;
            }
        }
    }

    static ThreadInfo* FindNextThread() {
        for(auto a = g_ThreadList.begin(); a != g_ThreadList.end(); ++a) {
            ThreadInfo& p = *a;
            if(p.blockEvent.type == ThreadBlockEvent::TYPE_NONE) {
                g_ThreadList.erase(a);
                g_ThreadList.push_back(&p);
                return &p;
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
        p->registers.rflags = 0b000000000001000000000;

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

    void ThreadWait(uint64 ms, IDT::Registers* returnregs)
    {
        IDT::DisableInterrupts();
        g_CPUData.currentThread->blockEvent.type = ThreadBlockEvent::TYPE_WAIT;
        g_CPUData.currentThread->blockEvent.wait.remainingMillis = ms;
        
        SaveThreadInfo(returnregs);

        ThreadInfo* next = FindNextThread();
        SetContext(next, returnregs);
        ReturnToThread(returnregs);
    }

    void KernelThreadWait(uint64 ms)
    {
        IDT::DisableInterrupts();
        g_CPUData.currentThread->blockEvent.type = ThreadBlockEvent::TYPE_WAIT;
        g_CPUData.currentThread->blockEvent.wait.remainingMillis = ms;
        IDT::Registers* myRegs = &g_CPUData.currentThread->registers;

        ThreadInfo* nextThread = FindNextThread();

        IDT::Registers nextRegs;
        SetContext(nextThread, &nextRegs);
        
        SyscallContextSwitch(myRegs, &nextRegs);

        IDT::EnableInterrupts();
    }

    void ThreadExit(uint64 code)
    {
        IDT::DisableInterrupts();

        ThreadInfo* me = g_CPUData.currentThread;

        if(me->process != nullptr) {
            me->process->lock.SpinLock();
            me->process->threads.erase(me);
            if(me->process->threads.empty()) {
                MemoryManager::FreeProcessMap(me->process->pml4Entry);
                delete me->process;
            } else {
                me->process->lock.Unlock();
            }
        }

        //void* kStack = (void*)(me->kernelStack - KernelStackSize);
        //MemoryManager::FreePages(kStack, KernelStackPages);

        g_ThreadList.erase(me);
        
        IDT::Registers regs;
        ThreadInfo* next = FindNextThread();
        SetContext(next, &regs);

        delete me;
        
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
        ThreadInfo* tInfo = new ThreadInfo();
        memset(tInfo, 0, sizeof(ThreadInfo));
        tInfo->tid = g_TIDCounter++;
        tInfo->process = g_CPUData.currentThread->process;
        tInfo->kernelStack = (uint64)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages(KernelStackPages)) + KernelStackSize;
        tInfo->userGSBase = 0;
        tInfo->registers.cs = GDT::UserCode;
        tInfo->registers.ds = GDT::UserData;
        tInfo->registers.ss = GDT::UserData;
        tInfo->registers.rflags = 0b000000000001000000000;
        tInfo->registers.rip = entry;
        tInfo->registers.userrsp = stack;

        if(tInfo->process != nullptr) {
            tInfo->process->lock.SpinLock();
            tInfo->process->threads.push_back(tInfo);
            tInfo->process->lock.Unlock();
        }

        IDT::DisableInterrupts();
        g_ThreadList.push_back(tInfo);
        uint64 res = tInfo->tid;
        IDT::EnableInterrupts();

        return res;
    }

}