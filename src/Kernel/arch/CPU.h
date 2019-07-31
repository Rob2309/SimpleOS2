#pragma once

namespace CPU {

    enum Flags {
        FLAGS_CF = (1 << 0),
        FLAGS_PF = (1 << 2),
        FLAGS_AF = (1 << 4),
        FLAGS_ZF = (1 << 6),
        FLAGS_SF = (1 << 7),
        FLAGS_TF = (1 << 8),
        FLAGS_IF = (1 << 9),
        FLAGS_DF = (1 << 10),
        FLAGS_OF = (1 << 11),
        FLAGS_IOPL = (3 << 12),
        FLAGS_NT = (1 << 14),
        FLAGS_RF = (1 << 16),
        FLAGS_VM = (1 << 17),
        FLAGS_AC = (1 << 18),
        FLAGS_VIF = (1 << 19),
        FLAGS_VIP = (1 << 20),
        FLAGS_ID = (1 << 21),
    };

    inline void CPUID(uint64 func, uint64 subFunc, uint64& eax, uint64& ebx, uint64& ecx, uint64& edx) {
        __asm__ __volatile__ (
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(func), "c"(subFunc)
        );
    }

}