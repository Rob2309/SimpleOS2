#include "SyscallHandler.h"

#include "arch/MSR.h"
#include "arch/GDT.h"
#include "klib/stdio.h"
#include "terminal/terminal.h"
#include "klib/string.h"

#include "SyscallFunctions.h"
#include "interrupts/IDT.h"
#include "scheduler/Scheduler.h"
#include "memory/MemoryManager.h"
#include "klib/memory.h"

#include "fs/VFS.h"

#include "exec/ExecHandler.h"

#include "SyscallDefine.h"

namespace SyscallHandler {

    extern "C" void SyscallEntry();

    extern "C" int SYSCALL_ARRAY_START;
    extern "C" int SYSCALL_ARRAY_END;

    typedef uint64 (*SyscallFunc)(uint64 arg1, uint64 arg2, uint64 arg3, uint64 arg4, SyscallState* state);

    static SyscallFunc g_Functions[1024] = { nullptr };

    void Init() {
        uint64* arr = (uint64*)&SYSCALL_ARRAY_START;
        uint64* arrEnd = (uint64*)&SYSCALL_ARRAY_END;
        for(; arr != arrEnd; arr += 2) {
            g_Functions[arr[1]] = (SyscallFunc)(arr[0]);
        }
    }

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

    static uint64 DoFork(SyscallState* state)
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

        return Scheduler::CloneThread(false, false, false, &childRegs);
    }
    SYSCALL_DEFINE0(syscall_fork) {
        return DoFork(state);
    }

    static uint64 DoExec(const char* cmdLine) {
        if(!MemoryManager::IsUserPtr(cmdLine))
            Scheduler::ThreadExit(1);

        char cmdBuffer[256];
        if(!kpathcpy_usersafe(cmdBuffer, cmdLine))
            Scheduler::ThreadExit(1);
        int l = kstrlen(cmdBuffer);

        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();

        char* filePath = cmdBuffer;
        char* arg = nullptr;
        for(int i = 0; i < l; i++) {
            if(filePath[i] == ' ') {
                filePath[i] = '\0';
                arg = &filePath[i+1];
                break;
            }
        }

        int argc = 1;
        char* argv[] = { nullptr, nullptr };
        argv[0] = filePath;

        if(arg != nullptr) {
            argv[1] = arg;
            argc = 2;
        }

        uint64 file;
        int64 error = VFS::Open(tInfo->user, filePath, VFS::OpenMode_Read, file);
        if(error != OK)
            return error;
        
        VFS::NodeStats stats;
        VFS::Stat(tInfo->user, filePath, stats);

        uint8* buffer = new uint8[stats.size];
        VFS::Read(file, buffer, stats.size);
        VFS::Close(file);

        uint64 pml4Entry = MemoryManager::CreateProcessMap();
        IDT::Registers regs;
        if(!ExecHandlerRegistry::Prepare(buffer, stats.size, pml4Entry, &regs, argc, argv)) {
            delete[] buffer;
            MemoryManager::FreeProcessMap(pml4Entry);
            return ErrorInvalidPath;
        }

        delete[] buffer;
        Scheduler::ThreadExec(pml4Entry, &regs);
    }
    SYSCALL_DEFINE1(syscall_exec, const char* path) {
        return DoExec(path);
    }

    extern "C" uint64 SyscallDispatcher(uint64 func, uint64 arg1, uint64 arg2, uint64 arg3, uint64 arg4, SyscallState* state)
    {
        uint64 res = 0;

        if(g_Functions[func] != nullptr) {
            res = g_Functions[func](arg1, arg2, arg3, arg4, state);
        } else {
            klog_error("Fault", "Thread %i called invalid syscall %i", Scheduler::ThreadGetTID(), func);
            Scheduler::ThreadExit(1);
        }

        Scheduler::ThreadCheckFlags();
        return res;
    }

}