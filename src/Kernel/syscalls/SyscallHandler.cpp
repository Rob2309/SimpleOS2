#include "SyscallHandler.h"

#include "arch/MSR.h"
#include "arch/GDT.h"
#include "klib/stdio.h"
#include "terminal/terminal.h"

#include "Syscall.h"
#include "interrupts/IDT.h"
#include "scheduler/Scheduler.h"
#include "memory/MemoryManager.h"
#include "klib/memory.h"

#include "scheduler/Process.h"

#include "fs/VFS.h"

#include "scheduler/ELF.h"

namespace SyscallHandler {

    extern "C" void SyscallEntry();

    void InitCore()
    {
        uint64 eferVal = MSR::Read(MSR::RegEFER);
        eferVal |= 1;
        MSR::Write(MSR::RegEFER, eferVal);

        uint64 starVal = ((uint64)(GDT::UserCode - 16) << 48) | ((uint64)GDT::KernelCode << 32);
        MSR::Write(MSR::RegSTAR, starVal);

        uint64 lstarVal = (uint64)&SyscallEntry;
        MSR::Write(MSR::RegLSTAR, lstarVal);

        uint64 cstarVal = 0;
        MSR::Write(MSR::RegCSTAR, cstarVal);

        uint64 sfmaskVal = 0b000000000001000000000;     // disable interrupts on syscall
        MSR::Write(MSR::RegSFMASK, sfmaskVal);
    }

    struct __attribute__((packed)) State
    {
        uint64 userrip, userrsp, userrbp, userflags;
        uint64 userrbx, userr12, userr13, userr14, userr15;
    };

    static uint64 DoFork(State* state)
    {
        IDT::Registers childRegs;
        kmemset(&childRegs, 0, sizeof(IDT::Registers));
        childRegs.rip = state->userrip;
        childRegs.userrsp = state->userrsp;
        childRegs.rbp = state->userrbp;
        childRegs.rflags = state->userflags;
        childRegs.rbx = state->userrbx;
        childRegs.r12 = state->userr12;
        childRegs.r13 = state->userr13;
        childRegs.r14 = state->userr14;
        childRegs.r15 = state->userr15;
        childRegs.ss = GDT::UserData;
        childRegs.cs = GDT::UserCode;
        childRegs.ds = GDT::UserData;
        childRegs.rax = 0;

        return Scheduler::CloneProcess(&childRegs);
    }

    static void SendSignal(uint64 signal) {
        IDT::DisableInterrupts();
        Scheduler::ThreadReturnToSignalHandler(signal);
    }

    extern "C" uint64 SyscallDispatcher(uint64 func, uint64 arg1, uint64 arg2, uint64 arg3, uint64 arg4, State* state)
    {
        switch(func) {
        case Syscall::FunctionPrint: {
                const char* msg = (const char*)arg1;
                if(!MemoryManager::IsUserPtr(msg))
                    SendSignal(SignalAbort);

                kprintf_colored(200, 50, 50, "[%i.%i] ", Scheduler::ThreadGetPID(), Scheduler::ThreadGetTID());
                kprintf((const char*)arg1); 
                break;
            } break;
        case Syscall::FunctionWait: Scheduler::ThreadWait(arg1); break;
        case Syscall::FunctionGetPID: return Scheduler::ThreadGetPID(); break;
        case Syscall::FunctionExit: 
            kprintf_colored(200, 50, 50, "[%i.%i] ", Scheduler::ThreadGetPID(), Scheduler::ThreadGetTID());
            kprintf("Exiting with code %i\n", arg1);
            Scheduler::ThreadExit(arg1); 
            break;
        case Syscall::FunctionFork: return DoFork(state); break;
        case Syscall::FunctionCreateThread: {
                uint64 entry = arg1;
                uint64 stack = arg2;
                if(!MemoryManager::IsUserPtr((void*)entry) || !MemoryManager::IsUserPtr((void*)stack))
                    SendSignal(SignalAbort);
                Scheduler::ThreadCreateThread(entry, stack); 
            } break;
        case Syscall::FunctionExec: {
                const char* filePath = (const char*)arg1;
                if(!MemoryManager::IsUserPtr(filePath))
                    SendSignal(SignalAbort);

                uint64 file = VFS::Open(filePath);
                if(file == 0)
                    return 0;
                
                VFS::NodeStats stats;
                VFS::Stat(file, &stats);

                uint8* buffer = new uint8[stats.size];
                VFS::Read(file, buffer, stats.size);
                VFS::Close(file);

                uint64 pml4Entry;
                IDT::Registers regs;
                if(!PrepareELF(buffer, pml4Entry, regs)) {
                    delete[] buffer;
                    return 0;
                }

                delete[] buffer;
                Scheduler::ProcessExec(pml4Entry, &regs);
            } break;

        case Syscall::FunctionCreateFile: {
                const char* filePath = (const char*)arg1;
                if(!MemoryManager::IsUserPtr(filePath))
                    SendSignal(SignalAbort);
                return VFS::CreateFile(filePath); 
            } break;
        case Syscall::FunctionCreateFolder: {
                const char* filePath = (const char*)arg1;
                if(!MemoryManager::IsUserPtr(filePath))
                    SendSignal(SignalAbort);
                return VFS::CreateFolder(filePath); 
            } break;
        case Syscall::FunctionCreateDeviceFile: {
                const char* filePath = (const char*)arg1;
                if(!MemoryManager::IsUserPtr(filePath))
                    SendSignal(SignalAbort);
                return VFS::CreateDeviceFile(filePath, arg2, arg3); 
            } break;
        case Syscall::FunctionCreatePipe: {
                uint64 sysRead, sysWrite;
                VFS::CreatePipe(&sysRead, &sysWrite);
                *(uint64*)arg1 = Scheduler::ProcessAddFileDescriptor(sysRead);
                *(uint64*)arg2 = Scheduler::ProcessAddFileDescriptor(sysWrite);
            } break;
        case Syscall::FunctionDelete: {
                const char* filePath = (const char*)arg1;
                if(!MemoryManager::IsUserPtr(filePath))
                    SendSignal(SignalAbort);
                return VFS::Delete(filePath); 
            } break;
        case Syscall::FunctionOpen: {
                uint64 sysDesc = VFS::Open((const char*)arg1);
                if(sysDesc == 0)
                    return 0;
                uint64 desc = Scheduler::ProcessAddFileDescriptor(sysDesc);
                return desc;
            } break;
        case Syscall::FunctionClose: Scheduler::ProcessCloseFileDescriptor(arg1); break;
        case Syscall::FunctionRead: {
                void* buffer = (void*)arg2;
                if(!MemoryManager::IsUserPtr(buffer))
                    SendSignal(SignalAbort);
                return VFS::Read(Scheduler::ProcessGetSystemFileDescriptor(arg1), buffer, arg3); 
            } break;
        case Syscall::FunctionWrite: {
                const void* buffer = (const void*)arg2;
                if(!MemoryManager::IsUserPtr(buffer))
                    SendSignal(SignalAbort);
                return VFS::Write(Scheduler::ProcessGetSystemFileDescriptor(arg1), buffer, arg3); 
            } break;
        case Syscall::FunctionMount: { 
                const char* mountPoint = (const char*)arg1;
                const char* fsID = (const char*)arg2;
                if(!MemoryManager::IsUserPtr(mountPoint) || !MemoryManager::IsUserPtr(fsID))
                    SendSignal(SignalAbort);
                return VFS::Mount(mountPoint, fsID); 
            } break;
        case Syscall::FunctionMountDev: {
                const char* mountPoint = (const char*)arg1;
                const char* fsID = (const char*)arg2;
                const char* devFile = (const char*) arg3;
                if(!MemoryManager::IsUserPtr(mountPoint) || !MemoryManager::IsUserPtr(fsID) || !MemoryManager::IsUserPtr(devFile))
                    SendSignal(SignalAbort);
                return VFS::Mount(mountPoint, fsID, devFile);
            } break;

        case Syscall::FunctionAllocPages: {
                char* pageBase = (char*)arg1;
                if(!MemoryManager::IsUserPtr(pageBase))
                    SendSignal(SignalAbort);
                for(uint64 i = 0; i < arg2; i++) {
                    MemoryManager::MapProcessPage(pageBase + i * 4096);
                }
                return true;
            } break;
        case Syscall::FunctionFreePages: {
                char* pageBase = (char*)arg1;
                if(!MemoryManager::IsUserPtr(pageBase))
                    SendSignal(SignalAbort);
                for(uint64 i = 0; i < arg2; i++) {
                    MemoryManager::UnmapProcessPage(pageBase + i * 4096);
                }
            } break;
        }

        return 0;
    }

}