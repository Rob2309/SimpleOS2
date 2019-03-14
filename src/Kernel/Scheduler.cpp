#include "Scheduler.h"

#include "MemoryManager.h"
#include "memutil.h"
#include "conio.h"

#include "terminal.h"

struct ProcessInfo
{
    ProcessInfo* next;

    // Process info
    enum {
        STATUS_READY,
        STATUS_RUNNING,
    } status;

    uint64 pml4Entry;

    uint64 pid;

    uint64 waitEnd;

    // Process state
    IDT::Registers registers;
};

extern uint64 g_TimeCounter;

namespace Scheduler {

    static ProcessInfo* g_IdleProcess;
    static uint64 g_IdleProcessStack[256];

    static ProcessInfo* g_RunningProcess;
    static ProcessInfo* g_ProcessList = nullptr;

    static uint64 g_PIDCounter = 1;

    ProcessInfo* RegisterProcessInternal(uint64 pml4Entry, uint64 rsp, uint64 rip, bool user) {
        ProcessInfo* p = (ProcessInfo*)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages());
        memset(p, 0, sizeof(ProcessInfo));

        p->pml4Entry = pml4Entry;
        p->status = ProcessInfo::STATUS_READY;
        p->pid = g_PIDCounter++;

        p->registers.userrsp = rsp;
        p->registers.rip = rip;
        p->registers.cs = user ? 0x1B : 0x08;
        p->registers.ds = user ? 0x23 : 0x10;
        p->registers.ss = user ? 0x23 : 0x10;
        p->registers.rflags = 0b000000000001000000000;
        
        p->next = g_ProcessList;
        g_ProcessList = p;

        return p;
    }

    uint64 RegisterProcess(uint64 pml4Entry, uint64 rsp, uint64 rip, bool user)
    {
        return RegisterProcessInternal(pml4Entry, rsp, rip, user)->pid;
    }
    
    static ProcessInfo* FindNextProcess(ProcessInfo* searchStart) {
        ProcessInfo* res = searchStart;
        while(true) {
            if(res->waitEnd < g_TimeCounter)
                return res;

            res = res->next;
            if(res == nullptr)
                res = g_ProcessList;
            if(res == searchStart)
                break;
        }
        return g_IdleProcess;
    }

    void Tick(IDT::Registers* regs)
    {
        g_RunningProcess->status = ProcessInfo::STATUS_READY;
        g_RunningProcess->registers = *regs; // save all registers in process info
        
        // Find next process to execute
        ProcessInfo* nextProcess;
        if(g_RunningProcess == g_IdleProcess || g_RunningProcess->next == nullptr)
            nextProcess = FindNextProcess(g_ProcessList);
        else
            nextProcess = FindNextProcess(g_RunningProcess->next);

        g_RunningProcess = nextProcess;
        g_RunningProcess->status = ProcessInfo::STATUS_RUNNING;
        // Load paging structures for the process
        MemoryManager::SwitchProcessMap(g_RunningProcess->pml4Entry);
        // Load registers from saved state
        *regs = g_RunningProcess->registers;
    }

    // This is the "process" that will run, when no other process is ready to run
    // it will just hlt and wait for the next interrupt
    static void IdleProcess()
    {
        while(true) {
            __asm__ __volatile__ ("hlt");
        }
    }

    void Start()
    {
        // Init idle process
        ProcessInfo* p = (ProcessInfo*)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages());
        memset(p, 0, sizeof(ProcessInfo));

        p->pml4Entry = 0;
        p->status = ProcessInfo::STATUS_READY;
        p->pid = 0;

        p->registers.userrsp = (uint64)&g_IdleProcessStack[256];
        p->registers.rip = (uint64)&IdleProcess;
        p->registers.cs = 0x08;
        p->registers.ds = 0x10;
        p->registers.rflags = 0b000000000001000000000;
        
        p->next = nullptr;

        g_IdleProcess = p;
        g_RunningProcess = g_IdleProcess;

        __asm__ __volatile__ (
            "pushq $0x10;"      // kernel data selector
            "pushq %0;"         // rsp
            "pushq %1;"         // rflags
            "pushq $0x08;"      // kernel code selector
            "pushq %2;"         // rip
            "movq $0x10, %%rax;"// load kernel data selectors
            "mov %%rax, %%ds;"
            "mov %%rax, %%es;"
            "mov %%rax, %%fs;"
            "mov %%rax, %%gs;"
            "iretq"             // "return" to idle process
            : : "r"(p->registers.userrsp), "r"(p->registers.rflags), "r"(p->registers.rip)
        );
    }

    void ProcessWait(uint64 ms)
    {
        g_RunningProcess->waitEnd = g_TimeCounter + ms;
    }

    void ProcessExit(uint64 code, IDT::Registers* regs)
    {
        uint64 pid = g_RunningProcess->pid;

        // Remove process from list
        if(g_RunningProcess == g_ProcessList) {
            g_ProcessList = g_ProcessList->next;
        } else {
            ProcessInfo* temp = g_ProcessList;
            while(temp->next != g_RunningProcess)
                temp = temp->next;
            temp->next = g_RunningProcess->next;
        }

        // Delete paging structures for this process
        MemoryManager::FreeProcessMap(g_RunningProcess->pml4Entry);

        // Switch to idle process
        g_RunningProcess = g_IdleProcess;
        *regs = g_IdleProcess->registers;

        printf("Process %i exited with code %i\n", pid, code);
    }

    void ProcessFork(IDT::Registers* regs)
    {
        // Copy process memory and paging structures
        uint64 pml4Entry = MemoryManager::ForkProcessMap();
        
        // Register an exact copy of the current process
        ProcessInfo* pInfo = RegisterProcessInternal(pml4Entry, regs->userrsp, regs->rip, true);
        // return child process pid to parent process
        regs->rax = pInfo->pid;
        pInfo->registers = *regs;
        // return zero to child process
        pInfo->registers.rax = 0;
    }

    uint64 GetCurrentPID()
    {
        return g_RunningProcess->pid;
    }

}