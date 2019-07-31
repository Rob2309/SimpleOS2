#include "Time.h"

#include "arch/MSR.h"
#include "arch/CPU.h"
#include "arch/port.h"

#include "klib/stdio.h"

namespace Time {

    static uint64 g_TSCTicksPerMilli;

    static void CalibrateTSC() {
        uint64 ticksPerMS = 0;
        for(int i = 0; i < 5; i++) {
            Port::OutByte(0x43, 0x30);

            constexpr uint16 pitTickInitCount = 39375; // 33 ms
            Port::OutByte(0x40, pitTickInitCount & 0xFF);
            Port::OutByte(0x40, (pitTickInitCount >> 8) & 0xFF);

            uint64 tscStart;
            uint64 edx, eax;
            __asm__ __volatile__ (
                "mfence;"
                "rdtsc;"
                "lfence"
                : "=d"(edx), "=a"(eax)
            );
            tscStart = (edx << 32) | eax;

            uint16 pitCurrentCount = 0;
            while(pitCurrentCount <= pitTickInitCount)
            {
                Port::OutByte(0x43, 0);
                pitCurrentCount = (uint16)Port::InByte(0x40) | ((uint16)Port::InByte(0x40) << 8);
            }

            uint64 tscEnd;
            __asm__ __volatile__ (
                "mfence;"
                "rdtsc;"
                "lfence"
                : "=d"(edx), "=a"(eax)
            );
            tscEnd = (edx << 32) | eax;

            uint64 elapsed = tscEnd - tscStart;
            ticksPerMS += elapsed / 33;
        }

        ticksPerMS /= 5;
        klog_info("Time", "TSC runs at %i kHz", ticksPerMS);
        g_TSCTicksPerMilli = ticksPerMS;
    }

    bool Init() {
        uint64 eax, ebx, ecx, edx;
        CPU::CPUID(1, 0, eax, ebx, ecx, edx);
        if((edx & (1 << 4)) == 0) {
            klog_fatal("Time", "CPU Time stamp counter not supported");
            return false;
        }

        CPU::CPUID(0x80000001, 0, eax, ebx, ecx, edx);
        if((edx & (1 << 27)) == 0) {
            klog_fatal("Time", "RDTSCP instruction not supported");
            return false;
        }

        CPU::CPUID(0x80000007, 0, eax, ebx, ecx, edx);
        if((edx & (1 << 8)) == 0) {
            klog_fatal("Time", "TSC is not invariant");
            return false;
        }

        CalibrateTSC();

        return true;
    }

    uint64 GetTSCTicksPerMilli() {
        return g_TSCTicksPerMilli;
    }

    uint64 GetTSC() {
        uint64 edx, eax;
        __asm__ __volatile__ (
            "rdtsc"
            : "=d"(edx), "=a"(eax)
        );
        return ((edx << 32) & 0xFFFFFFFF00000000) | (eax & 0xFFFFFFFF);
    }

}