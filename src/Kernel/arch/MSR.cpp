#include "MSR.h"

namespace MSR
{
    void Write(uint32 reg, uint64 val)
    {
        __asm__ __volatile__ (
            "wrmsr"
            : :
            "c"(reg),
            "d"((val >> 32) & 0xFFFFFFFF),
            "a"(val & 0xFFFFFFFF)
        );
    }

    uint64 Read(uint32 reg)
    {
        uint64 eax, edx;
        __asm__ __volatile__ (
            "rdmsr"
            : "=a"(eax), "=d"(edx)
            : "c"(reg)
        );
        return (edx << 32) | eax;
    }
}