#include "Scheduler.h"

#include "Process.h"
#include "list.h"
#include "memutil.h"
#include "IDT.h"
#include "GDT.h"
#include "MemoryManager.h"
#include "MSR.h"
#include "APIC.h"

namespace Scheduler {

    extern "C" void IdleThread();

    static std::list<ThreadInfo*> g_ThreadList;

    static uint64 g_PIDCounter = 1;

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

    uint64 RegisterProcess(uint64 pml4Entry, uint64 kernelStack, IDT::Registers* regs)
    {
        ThreadInfo* p = new ThreadInfo();
        memset(p, 0, sizeof(ThreadInfo));

        p->pid = g_PIDCounter++;
        p->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        p->pml4Entry = pml4Entry;
        p->kernelStack = kernelStack;
        p->userGSBase = 0;
        p->registers = *regs;
        
        IDT::DisableInterrupts();
        g_ThreadList.push_back(p);
        uint64 ret = p->pid;
        IDT::EnableInterrupts();

        return ret;
    }
    uint64 RegisterProcess(uint64 pml4Entry, uint64 rsp, uint64 rip, bool user, uint64 kernelStack)
    {
        IDT::Registers regs;
        memset(&regs, 0, sizeof(IDT::Registers));
        regs.cs = user ? GDT::UserCode : GDT::KernelCode;
        regs.ds = user ? GDT::UserData : GDT::KernelData;
        regs.ss = user ? GDT::UserData : GDT::KernelData;
        regs.rflags = 0b000000000001000000000;
        regs.rip = rip;
        regs.userrsp = rsp;

        return RegisterProcess(pml4Entry, kernelStack, &regs);
    }

    uint64 CreateKernelThread(uint64 rip)
    {
        uint64 kernelStack = (uint64)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages(KernelStackPages)) + KernelStackSize;
        return RegisterProcess(0, kernelStack, rip, false, 0);
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
        MemoryManager::SwitchProcessMap(g_CPUData.currentThread->pml4Entry);
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

        p->pid = 0;
        p->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        p->pml4Entry = 0;
        p->kernelStack = 0;
        p->userGSBase = 0;

        p->registers.userrsp = 0;
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

    void ProcessWait(uint64 ms, IDT::Registers* returnregs)
    {
        IDT::DisableInterrupts();
        g_CPUData.currentThread->blockEvent.type = ThreadBlockEvent::TYPE_WAIT;
        g_CPUData.currentThread->blockEvent.wait.remainingMillis = ms;
        
        SaveThreadInfo(returnregs);

        ThreadInfo* next = FindNextThread();
        SetContext(next, returnregs);
        ReturnToThread(returnregs);
    }

    void KernelWait(uint64 ms)
    {
        IDT::DisableInterrupts();
        g_CPUData.currentThread->blockEvent.type = ThreadBlockEvent::TYPE_WAIT;
        g_CPUData.currentThread->blockEvent.wait.remainingMillis = ms;

        ThreadInfo* nextProcess = FindNextThread();

        IDT::Registers nextRegs;
        SetContext(nextProcess, &nextRegs);
        
        IDT::Registers* myRegs = &g_CPUData.currentThread->registers;
        SyscallContextSwitch(myRegs, &nextRegs);

        IDT::EnableInterrupts();
    }

    void ProcessExit(uint64 code)
    {
        ThreadInfo* me = g_CPUData.currentThread;

        MemoryManager::FreeProcessMap(me->pml4Entry);

        //void* kStack = (void*)(me->kernelStack - KernelStackSize);
        //MemoryManager::FreePages(kStack, KernelStackPages);

        IDT::DisableInterrupts();
        g_ThreadList.erase(me);
        
        IDT::Registers regs;
        ThreadInfo* next = FindNextThread();
        SetContext(next, &regs);

        delete me;
        
        ReturnToThread(&regs);
    }

    uint64 GetCurrentPID()
    {
        return g_CPUData.currentThread->pid;
    }

}