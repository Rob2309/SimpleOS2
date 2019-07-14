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

#include "exec/ExecHandler.h"

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

    extern "C" uint64 SyscallDispatcher(uint64 func, uint64 arg1, uint64 arg2, uint64 arg3, uint64 arg4, State* state)
    {
        Scheduler::ThreadSetUnkillable(true);
        uint64 res = 0;

        switch(func) {
        case Syscall::FunctionSetFS: 
            Scheduler::ThreadSetFS(arg1); 
            MSR::Write(MSR::RegFSBase, arg1);
            break;

        case Syscall::FunctionPrint: {
                const char* msg = (const char*)arg1;
                if(!MemoryManager::IsUserPtr(msg))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");

                kprintf(msg);
            } break;
        case Syscall::FunctionWait: Scheduler::ThreadWait(arg1); break;
        case Syscall::FunctionGetPID: res = Scheduler::ThreadGetPID(); break;
        case Syscall::FunctionExit: 
            Scheduler::ThreadExit(arg1);
            break;
        case Syscall::FunctionFork: res = DoFork(state); break;
        case Syscall::FunctionCreateThread: {
                uint64 entry = arg1;
                uint64 stack = arg2;
                uint64 arg = arg3;
                if(!MemoryManager::IsUserPtr((void*)entry) || !MemoryManager::IsUserPtr((void*)stack))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");
                res = Scheduler::ThreadCreateThread(entry, stack, arg); 
            } break;
        case Syscall::FunctionExec: {
                const char* filePath = (const char*)arg1;
                if(!MemoryManager::IsUserPtr(filePath))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");

                ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
                ProcessInfo* pInfo = tInfo->process;

                uint64 file;
                int64 error = VFS::Open(pInfo->owner, filePath, VFS::Permissions::Read, file);
                if(error != VFS::OK)
                    return error;
                
                VFS::NodeStats stats;
                VFS::Stat(file, &stats);

                uint8* buffer = new uint8[stats.size];
                VFS::Read(file, buffer, stats.size);
                VFS::Close(file);

                uint64 pml4Entry = MemoryManager::CreateProcessMap();
                IDT::Registers regs;
                if(!ExecHandlerRegistry::Prepare(buffer, stats.size, pml4Entry, &regs)) {
                    delete[] buffer;
                    MemoryManager::FreeProcessMap(pml4Entry);
                    return 0;
                }

                delete[] buffer;
                Scheduler::ProcessExec(pml4Entry, &regs);
            } break;

        case Syscall::FunctionCreateFile: {
                const char* filePath = (const char*)arg1;
                if(!MemoryManager::IsUserPtr(filePath))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");

                ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
                ProcessInfo* pInfo = tInfo->process;

                res = VFS::CreateFile(pInfo->owner, filePath, {3, 3, 3});
            } break;
        case Syscall::FunctionCreateFolder: {
                const char* filePath = (const char*)arg1;
                if(!MemoryManager::IsUserPtr(filePath))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");

                ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
                ProcessInfo* pInfo = tInfo->process;

                res = VFS::CreateFolder(pInfo->owner, filePath, {3, 3, 3}); 
            } break;
        case Syscall::FunctionCreateDeviceFile: {
                const char* filePath = (const char*)arg1;
                if(!MemoryManager::IsUserPtr(filePath))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");

                ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
                ProcessInfo* pInfo = tInfo->process;

                res = VFS::CreateDeviceFile(pInfo->owner, filePath, {3, 3, 3}, arg2, arg3); 
            } break;
        case Syscall::FunctionCreatePipe: {
                uint64 sysRead, sysWrite;

                ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
                ProcessInfo* pInfo = tInfo->process;

                VFS::CreatePipe(pInfo->owner, &sysRead, &sysWrite);
                *(int64*)arg1 = Scheduler::ProcessAddFileDescriptor(sysRead);
                *(int64*)arg2 = Scheduler::ProcessAddFileDescriptor(sysWrite);
            } break;
        case Syscall::FunctionDelete: {
                const char* filePath = (const char*)arg1;
                if(!MemoryManager::IsUserPtr(filePath))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");

                ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
                ProcessInfo* pInfo = tInfo->process;

                res = VFS::Delete(pInfo->owner, filePath); 
            } break;
        case Syscall::FunctionChangePermissions: {
                const char* filePath = (const char*)arg1;
                uint8 ownerPerm = arg2;
                uint8 groupPerm = arg3;
                uint8 otherPerm = arg4;
                if(!MemoryManager::IsUserPtr(filePath))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");

                ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
                ProcessInfo* pInfo = tInfo->process;

                res = VFS::ChangePermissions(pInfo->owner, filePath, { ownerPerm, groupPerm, otherPerm });
            } break;
        case Syscall::FunctionChangeOwner: {
                const char* filePath = (const char*)arg1;
                uint64 uid = arg2;
                uint64 gid = arg3;
                if(!MemoryManager::IsUserPtr(filePath))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");

                ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
                ProcessInfo* pInfo = tInfo->process;

                res = VFS::ChangeOwner(pInfo->owner, filePath, uid, gid);
            } break;
        case Syscall::FunctionOpen: {
                ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
                ProcessInfo* pInfo = tInfo->process;

                uint64 sysDesc;
                int64 error = VFS::Open(pInfo->owner, (const char*)arg1, arg2, sysDesc);
                if(error != VFS::OK) {
                    res = error;
                    break;
                }
                int64 desc = Scheduler::ProcessAddFileDescriptor(sysDesc);
                res = desc;
            } break;
        case Syscall::FunctionReplaceFD: res = Scheduler::ProcessReplaceFileDescriptor(arg1, arg2); break;
        case Syscall::FunctionReplaceFDPath: {
                ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
                ProcessInfo* pInfo = tInfo->process;

                uint64 sysDesc;
                int64 error = VFS::Open(pInfo->owner, (const char*)arg2, arg3, sysDesc);
                if(error != VFS::OK) {
                    res = error;
                    break;
                }
                
                res = Scheduler::ProcessReplaceFileDescriptorValue(arg1, sysDesc);
                if(res != VFS::OK) {
                    VFS::Close(sysDesc);
                }
            } break;
        case Syscall::FunctionClose: res = Scheduler::ProcessCloseFileDescriptor(arg1); break;
        case Syscall::FunctionRead: {
                void* buffer = (void*)arg2;
                if(!MemoryManager::IsUserPtr(buffer))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");

                uint64 sysDesc;
                int64 error = Scheduler::ProcessGetSystemFileDescriptor(arg1, sysDesc);
                if(error != VFS::OK) {
                    res = error;
                    break;
                }

                res = VFS::Read(sysDesc, buffer, arg3);
                if(res == VFS::ErrorInvalidBuffer)
                    Scheduler::ThreadKillProcess("InvalidUserPointer");
            } break;
        case Syscall::FunctionWrite: {
                const void* buffer = (const void*)arg2;
                if(!MemoryManager::IsUserPtr(buffer))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");

                uint64 sysDesc;
                int64 error = Scheduler::ProcessGetSystemFileDescriptor(arg1, sysDesc);
                if(error != VFS::OK) {
                    res = error;
                    break;
                }

                res = VFS::Write(sysDesc, buffer, arg3);
                if(res == VFS::ErrorInvalidBuffer)
                    Scheduler::ThreadKillProcess("InvalidUserPointer");
            } break;
        case Syscall::FunctionMount: { 
                const char* mountPoint = (const char*)arg1;
                const char* fsID = (const char*)arg2;
                if(!MemoryManager::IsUserPtr(mountPoint) || !MemoryManager::IsUserPtr(fsID))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");

                ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
                ProcessInfo* pInfo = tInfo->process;

                res = VFS::Mount(pInfo->owner, mountPoint, fsID); 
            } break;
        case Syscall::FunctionMountDev: {
                const char* mountPoint = (const char*)arg1;
                const char* fsID = (const char*)arg2;
                const char* devFile = (const char*) arg3;
                if(!MemoryManager::IsUserPtr(mountPoint) || !MemoryManager::IsUserPtr(fsID) || !MemoryManager::IsUserPtr(devFile))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");

                ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
                ProcessInfo* pInfo = tInfo->process;

                res = VFS::Mount(pInfo->owner, mountPoint, fsID, devFile);
            } break;

        case Syscall::FunctionAllocPages: {
                char* pageBase = (char*)arg1;
                if(!MemoryManager::IsUserPtr(pageBase))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");
                for(uint64 i = 0; i < arg2; i++) {
                    MemoryManager::MapProcessPage(pageBase + i * 4096);
                }
                res = 1;
            } break;
        case Syscall::FunctionFreePages: {
                char* pageBase = (char*)arg1;
                if(!MemoryManager::IsUserPtr(pageBase))
                    Scheduler::ThreadKillProcess("InvalidUserPointer");
                for(uint64 i = 0; i < arg2; i++) {
                    MemoryManager::UnmapProcessPage(pageBase + i * 4096);
                }
            } break;

        case Syscall::FunctionMoveToCore:
            Scheduler::ThreadMoveToCPU(arg1);
            break;
        }

        Scheduler::ThreadSetUnkillable(false);
        return res;
    }

}