#include "SyscallHandler.h"

#include "MSR.h"
#include "GDT.h"

namespace SyscallHandler {

    constexpr uint32 RegSTAR = 0x81;
    constexpr uint32 RegLSTAR = 0x82;
    constexpr uint32 RegCSTAR = 0x83;
    constexpr uint32 RegSFMASK = 0x84;

    extern "C" void SyscallEntry();

    void Init()
    {
        uint64 starVal = ((GDT::UserCode - 16) << 48) | (GDT::KernelCode << 32);
        WriteMSR(RegSTAR, starVal);

        uint64 lstarVal = (uint64)&SyscallEntry;
        WriteMSR(RegLSTAR, lstarVal);

        uint64 cstarVal = 0;
        WriteMSR(RegCSTAR, cstarVal);

        uint64 sfmaskVal = 0b000000000001000000000;
        WriteMSR(RegSFMASK, sfmaskVal);
    }

    struct Regs {
        uint64 rax, rbx, rdx, rdi, rsi, rbp;
        uint64 r8, r9, r10, r12, r13, r14, r15;

        uint64 returnflags;
        uint64 returnrip;
    };

    extern "C" void SyscallDispatcher()
    {

    }

}