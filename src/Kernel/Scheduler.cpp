#include "Scheduler.h"

#include "MemoryManager.h"
#include "memutil.h"
#include "conio.h"
#include "list.h"
#include "GDT.h"
#include "ArrayList.h"

#include "terminal.h"

#include "MSR.h"

#include "APIC.h"

static uint64 g_TimeCounter = 0;

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

    uint64 kernelgs;
    uint64 usergs;
    uint64 kernelStack;

    uint64 fileDescIDCounter;
    ArrayList<FileDescriptor> files;

    // Process state
    IDT::Registers registers;
};

namespace Scheduler {

    static ProcessInfo* g_IdleProcess;
    extern "C" void IdleProcess();

    static ProcessInfo* g_RunningProcess;
    static std::list<ProcessInfo*> g_ProcessList;

    static uint64 g_PIDCounter = 1;

    static ProcessInfo* FindNextProcess();
    ProcessInfo* RegisterProcessInternal(uint64 pml4Entry, uint64 rsp, uint64 rip, bool user, uint64 kernelStack);

    static struct {
        uint64 kernelStack;
    } g_CPUData;

    static void ForkCurrentProcess(IDT::Registers* regs, uint64 usergs)
    {
        IDT::Registers* returnregs = (IDT::Registers*)(regs->rdi);
        uint64 childKernelStack = regs->rsi;

        // Copy process memory and paging structures
        uint64 pml4Entry = MemoryManager::ForkProcessMap();
        
        // Register an exact copy of the current process
        ProcessInfo* pInfo = RegisterProcessInternal(pml4Entry, returnregs->userrsp, returnregs->rip, true, childKernelStack);
        // return child process pid to parent process
        regs->rax = pInfo->pid;

        pInfo->registers = *returnregs;
        // return zero to child process
        pInfo->registers.rax = 0;
        pInfo->usergs = usergs;
    }

    static void ExitCurrentProcess(IDT::Registers* regs)
    {
        uint64 pid = g_RunningProcess->pid;
        uint64 exitCode = regs->rdi;

        if(g_RunningProcess->pml4Entry != 0)
            MemoryManager::FreeProcessMap(g_RunningProcess->pml4Entry);

        g_ProcessList.erase(g_RunningProcess);
        delete g_RunningProcess;

        g_RunningProcess = g_IdleProcess;
        *regs = g_IdleProcess->registers;
        Tick(regs);

        printf("Process %i exited with code %i\n", pid, exitCode);
    }

    static void WaitCurrentProcess(IDT::Registers* regs)
    {
        g_RunningProcess->blockEvent = new ProcessEventWait(regs->rsi);
        Tick(regs);
    }

    static void SchedulerControlInterrupt(IDT::Registers* regs)
    {
        switch(regs->rax) {
        case ControlFuncExit: ExitCurrentProcess(regs); break;
        case ControlFuncWait: WaitCurrentProcess(regs); break;
        case ControlFuncFork: ForkCurrentProcess(regs, MSR::Read(MSR::RegKernelGSBase)); break;
        }
    }

    static void TimerEvent(IDT::Registers* regs)
    {
        g_TimeCounter += 10;
        Scheduler::Tick(regs);
    }

    ProcessInfo* RegisterProcessInternal(uint64 pml4Entry, uint64 rsp, uint64 rip, bool user, uint64 kernelStack) {
        ProcessInfo* p = new ProcessInfo();
        memset(p, 0, sizeof(ProcessInfo));

        p->fileDescIDCounter = 1;

        p->pml4Entry = pml4Entry;
        p->status = ProcessInfo::STATUS_READY;
        p->pid = g_PIDCounter++;

        p->kernelStack = kernelStack;
        p->kernelgs = (uint64)&g_CPUData;
        p->usergs = 0;

        p->registers.userrsp = rsp;
        p->registers.rip = rip;
        p->registers.cs = user ? GDT::UserCode : GDT::KernelCode;
        p->registers.ds = user ? GDT::UserData : GDT::KernelData;
        p->registers.ss = user ? GDT::UserData : GDT::KernelData;
        p->registers.rflags = 0b000000000001000000000;
        
        g_ProcessList.push_back(p);

        return p;
    }

    uint64 RegisterProcess(uint64 pml4Entry, uint64 rsp, uint64 rip, bool user, uint64 kernelStack)
    {
        return RegisterProcessInternal(pml4Entry, rsp, rip, user, kernelStack)->pid;
    }

    uint64 CreateKernelThread(uint64 rip)
    {
        uint64 kernelStack = (uint64)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages(3)) + Scheduler::KernelStackSize;
        return RegisterProcess(0, kernelStack, rip, false, 0);
    }

    static void UpdateEvents() {
        for(auto p : g_ProcessList) {
            if(p->status == ProcessInfo::STATUS_BLOCKED && p->blockEvent->Finished()) {
                delete p->blockEvent;
                p->blockEvent = nullptr;
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

    void Tick(IDT::Registers* regs)
    {
        g_RunningProcess->status = g_RunningProcess->blockEvent != nullptr ? ProcessInfo::STATUS_BLOCKED : ProcessInfo::STATUS_READY;
        g_RunningProcess->registers = *regs; // save all registers in process info
        g_RunningProcess->kernelgs = MSR::Read(MSR::RegKernelGSBase);
        g_RunningProcess->usergs = MSR::Read(MSR::RegGSBase);
        
        // Find next process to execute
        UpdateEvents();
        ProcessInfo* nextProcess = FindNextProcess();

        g_RunningProcess = nextProcess;
        g_RunningProcess->status = ProcessInfo::STATUS_RUNNING;
        // Load paging structures for the process
        MemoryManager::SwitchProcessMap(g_RunningProcess->pml4Entry);
        // Load registers from saved state
        *regs = g_RunningProcess->registers;
        g_CPUData.kernelStack = g_RunningProcess->kernelStack;
        MSR::Write(MSR::RegKernelGSBase, g_RunningProcess->kernelgs);
        MSR::Write(MSR::RegGSBase, g_RunningProcess->usergs);
    }

    void Start()
    {
        APIC::SetTimerEvent(TimerEvent);
        IDT::SetISR(ISRNumbers::SchedulerControl, SchedulerControlInterrupt);

        // Init idle process
        ProcessInfo* p = new ProcessInfo();
        memset(p, 0, sizeof(ProcessInfo));

        p->pml4Entry = 0;
        p->status = ProcessInfo::STATUS_READY;
        p->pid = 0;

        p->usergs = 0;
        p->kernelgs = 0;
        p->kernelStack = 0;

        p->registers.userrsp = 0;
        p->registers.rip = (uint64)&IdleProcess;
        p->registers.cs = GDT::KernelCode;
        p->registers.ds = GDT::KernelData;
        p->registers.ss = GDT::KernelData;
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
            "mov $0, %%rax;"    // This should not be required, but if left out, qemu always treats gs.base as zero, even when msr GS_BASE is set to non-zero values
            "mov %%rax, %%fs;"
            "mov %%rax, %%gs;"
            "iretq"             // "return" to idle process
            : : "r"(p->kernelgs), "r"(p->registers.rflags), "r"(p->registers.rip)
        );
    }

    void ProcessWait(uint64 ms)
    {
        __asm__ __volatile__ (
            "int $127"
            : : "a"(ControlFuncWait), "S"(ms)
        );
    }

    void ProcessYield()
    {
        __asm__ __volatile__ ("int $100");
    }

    uint64 ProcessFork(IDT::Registers* regs, uint64 kernelStack)
    {
        uint64 ret;
        __asm__ __volatile__ (
            "int $127"
            : "=a"(ret)
            : "a"(ControlFuncFork), "D"(regs), "S"(kernelStack)
        );
        return ret;
    }

    void ProcessExit(uint64 code)
    {
        __asm__ __volatile__ (
            "int $127"
            : : "a"(ControlFuncExit), "S"(code)
        );
    }

    uint64 ProcessAddFileDesc(uint64 node, bool read, bool write) {
        FileDescriptor desc;
        desc.id = g_RunningProcess->fileDescIDCounter++;
        desc.node = node;
        desc.read = read;
        desc.write = write;
        g_RunningProcess->files.push_back(desc);
        return desc.id;
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