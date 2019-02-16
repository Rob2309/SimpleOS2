#include "Scheduler.h"

#include "MemoryManager.h"
#include "memutil.h"
#include "conio.h"

struct ProcessInfo
{
    ProcessInfo* next;

    // Process info
    enum {
        STATUS_READY,
        STATUS_RUNNING,
    } status;

    uint64 pml4Entry;

    // Process state
    IDT::Registers registers;
};

namespace Scheduler {

    static ProcessInfo* g_RunningProcess;
    static ProcessInfo* g_ProcessList = nullptr;

    void RegisterProcess(uint64 pml4Entry, uint64 rsp, uint64 rip, bool user)
    {
        ProcessInfo* p = (ProcessInfo*)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages());
        memset(p, 0, sizeof(ProcessInfo));

        p->pml4Entry = pml4Entry;
        p->status = ProcessInfo::STATUS_READY;

        p->registers.userrsp = rsp;
        p->registers.rip = rip;
        p->registers.cs = user ? 0x1B : 0x08;
        p->registers.ds = user ? 0x23 : 0x10;
        
        p->next = g_ProcessList;
        g_ProcessList = p;
    }

    void Tick(IDT::Registers* regs)
    {
        if(g_RunningProcess == nullptr) {
            g_RunningProcess = g_ProcessList;
        } else {
            g_RunningProcess->status = ProcessInfo::STATUS_READY;
            g_RunningProcess->registers = *regs;
        }
        
        g_RunningProcess = g_RunningProcess->next;
        if(g_RunningProcess == nullptr)
            g_RunningProcess = g_ProcessList;

        g_RunningProcess->status = ProcessInfo::STATUS_RUNNING;
        MemoryManager::SwitchProcessMap(g_RunningProcess->pml4Entry);
        *regs = g_RunningProcess->registers;

        printf("Switching process\n");
    }

}