#include "Scheduler.h"

#include "MemoryManager.h"
#include "memutil.h"
#include "conio.h"
#include "list.h"
#include "ArrayList.h"

#include "terminal.h"

extern uint64 g_TimeCounter;

class ProcessEvent
{
public:
    virtual bool Finished() const = 0;
};

class ProcessEventWait : public ProcessEvent
{
public:
    ProcessEventWait(uint64 duration) : m_EndTime(g_TimeCounter + duration) { }
    bool Finished() const override { return g_TimeCounter >= m_EndTime; }
private:
    uint64 m_EndTime;
};

struct ProcessInfo
{
    // Process info
    enum {
        STATUS_READY,
        STATUS_BLOCKED,
        STATUS_RUNNING,
    } status;

    uint64 pml4Entry;

    uint64 pid;

    ProcessEvent* blockEvent;

    uint64 fileDescIDCounter;
    ArrayList<FileDescriptor> files;

    // Process state
    IDT::Registers registers;
};

namespace IDT {
    extern uint8 g_InterruptStack[4096];
}

namespace Scheduler {

    static ProcessInfo* g_IdleProcess;
    extern "C" void IdleProcess();

    static ProcessInfo* g_RunningProcess;
    static std::list<ProcessInfo*> g_ProcessList;

    static uint64 g_PIDCounter = 1;

    ProcessInfo* RegisterProcessInternal(uint64 pml4Entry, uint64 rsp, uint64 rip, bool user) {
        ProcessInfo* p = new ProcessInfo();
        memset(p, 0, sizeof(ProcessInfo));

        p->fileDescIDCounter = 1;

        p->pml4Entry = pml4Entry;
        p->status = ProcessInfo::STATUS_READY;
        p->pid = g_PIDCounter++;

        p->registers.userrsp = rsp;
        p->registers.rip = rip;
        p->registers.cs = user ? 0x1B : 0x08;
        p->registers.ds = user ? 0x23 : 0x10;
        p->registers.ss = user ? 0x23 : 0x10;
        p->registers.rflags = 0b000000000001000000000;
        
        g_ProcessList.push_back(p);

        return p;
    }

    uint64 RegisterProcess(uint64 pml4Entry, uint64 rsp, uint64 rip, bool user)
    {
        return RegisterProcessInternal(pml4Entry, rsp, rip, user)->pid;
    }

    static void UpdateEvents() {
        for(auto p : g_ProcessList) {
            if(p->status == ProcessInfo::STATUS_BLOCKED && p->blockEvent->Finished()) {
                delete p->blockEvent;
                p->blockEvent == nullptr;
                p->status = ProcessInfo::STATUS_READY;
            }
        }
    }

    static ProcessInfo* FindNextProcess() {
        for(auto a = g_ProcessList.begin(); a != g_ProcessList.end(); ++a) {
            ProcessInfo* p = *a;
            if(p->status == ProcessInfo::STATUS_READY) {
                g_ProcessList.erase(a);
                g_ProcessList.push_back(p);
                return p;
            }
        }

        return g_IdleProcess;
    }

    void Tick(IDT::Registers* regs, bool processBlocked)
    {
        g_RunningProcess->status = processBlocked ? ProcessInfo::STATUS_BLOCKED : ProcessInfo::STATUS_READY;
        g_RunningProcess->registers = *regs; // save all registers in process info
        
        // Find next process to execute
        UpdateEvents();
        ProcessInfo* nextProcess = FindNextProcess();

        g_RunningProcess = nextProcess;
        g_RunningProcess->status = ProcessInfo::STATUS_RUNNING;
        // Load paging structures for the process
        MemoryManager::SwitchProcessMap(g_RunningProcess->pml4Entry);
        // Load registers from saved state
        *regs = g_RunningProcess->registers;
    }

    void Start()
    {
        // Init idle process
        ProcessInfo* p = new ProcessInfo();
        memset(p, 0, sizeof(ProcessInfo));

        p->pml4Entry = 0;
        p->status = ProcessInfo::STATUS_READY;
        p->pid = 0;

        p->registers.userrsp = (uint64)&IDT::g_InterruptStack[sizeof(IDT::g_InterruptStack)];
        p->registers.rip = (uint64)&IdleProcess;
        p->registers.cs = 0x08;
        p->registers.ds = 0x10;
        p->registers.ss = 0x10;
        p->registers.rflags = 0b000000000001000000000;

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
        g_RunningProcess->blockEvent = new ProcessEventWait(ms);
    }

    void ProcessExit(uint64 code, IDT::Registers* regs)
    {
        uint64 pid = g_RunningProcess->pid;

        // Remove process from list
        g_ProcessList.erase(g_RunningProcess);

        // Delete paging structures for this process
        MemoryManager::FreeProcessMap(g_RunningProcess->pml4Entry);

        // Switch to idle process
        delete g_RunningProcess;
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

    uint64 ProcessAddFileDesc(uint64 node, bool read, bool write) {
        FileDescriptor desc;
        desc.id = g_RunningProcess->fileDescIDCounter++;
        desc.node = node;
        desc.read = read;
        desc.write = write;
        g_RunningProcess->files.push_back(desc);
    }

    void ProcessRemoveFileDesc(uint64 desc) {
        for(auto a = g_RunningProcess->files.begin(); a != g_RunningProcess->files.end(); ++a) {
            if(a->id == desc) {
                g_RunningProcess->files.erase(a);
                return;
            }
        }
    }

    FileDescriptor* ProcessGetFileDesc(uint64 id) {
        for(auto a = g_RunningProcess->files.begin(); a != g_RunningProcess->files.end(); ++a) {
            if(a->id == id) {
                return &(*a);
            }
        }
        return nullptr;
    }

    uint64 GetCurrentPID()
    {
        return g_RunningProcess->pid;
    }

}