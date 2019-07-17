#include "SSE.h"

#include "types.h"
#include "klib/stdio.h"

namespace SSE {

    static bool g_ExtendedSSE;
    static uint64 g_FPUBlockSize;

    static void CPUID(uint64 func, uint64 subFunc, uint64& eax, uint64& ebx, uint64& ecx, uint64& edx) {
        __asm__ __volatile__ (
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(func), "c"(subFunc)
        );
    }

    static bool CheckFeatures() {
        uint64 eax, ebx, ecx, edx;
        CPUID(0x1, 0x0, eax, ebx, ecx, edx);

        if(!(edx & (1 << 25))) {    // no SSE1
            klog_fatal("SSE", "SSE1 not supported");
            return false;
        }
        if(!(edx & (1 << 26))) {    // no SSE2
            klog_fatal("SSE", "SSE2 not supported");
            return false;
        }
        if(!(edx & (1 << 24))) {    // no FXSAVE / FXRSTOR
            klog_fatal("SSE", "FXSAVE/FXRSTOR not supported");
            return false;
        }

        bool xsave = false;

        if(ecx & (1 << 26)) {    // XSAVE / XRSTOR supported
            klog_info("SSE", "XSAVE/XRSTOR supported");
            xsave = true;
        }

        CPUID(0xD, 0x0, eax, ebx, ecx, edx);
        if(xsave && (eax & (1 << 2))) {
            klog_info("SSE", "Extended SSE supported");
            g_ExtendedSSE = true;
        }

        return true;
    }

    bool Init() {
        if(!CheckFeatures())
            return false;

        klog_info("SSE", "SSE initialized");
        klog_info("SSE", "FPU Block size: %i bytes", g_FPUBlockSize);

        return true;
    }

    void InitCore() {
        uint64 rax __attribute__((aligned(16)));

        // Enable FXSAVE/FXRSTOR and Exceptions
        __asm__ __volatile__( "movq %%cr4, %0" : "=r"(rax) );
        rax |= (1 << 9); 
        rax |= (1 << 10);
        if(g_ExtendedSSE) {
            rax |= (1 << 18);
        }
        __asm__ __volatile__( "movq %0, %%cr4" : : "r"(rax) );

        // Clear EM, set MP, clear TS
        __asm__ __volatile__( "movq %%cr0, %0" : "=r"(rax) );
        rax &= ~(1 << 2);
        rax |= (1 << 1);
        rax &= ~(1 << 3);
        __asm__ __volatile__( "movq %0, %%cr0" : : "r"(rax) );

        if(g_ExtendedSSE) {
            __asm__ __volatile__ ( "xsetbv" : : "d"(0), "a"(0b111), "c"(0) );

            uint64 rax, rbx, rcx, rdx;
            CPUID(0xD, 0, rax, rbx, rcx, rdx);
            g_FPUBlockSize = rbx;
        } else {
            g_FPUBlockSize = 512;
        }

        uint64 mxcsr __attribute__((aligned(64))) = 0;
        mxcsr |= (1 << 12);
        mxcsr |= (1 << 11);
        mxcsr |= (1 << 10);
        mxcsr |= (1 << 9);
        mxcsr |= (1 << 8);
        mxcsr |= (1 << 7);
        __asm__ __volatile__ ("ldmxcsr (%0)" : : "r"(&mxcsr) );
    }

    uint64 GetFPUBlockSize() {
        return g_FPUBlockSize;
    }
    void SaveFPUBlock(char* buffer) {
        if(g_ExtendedSSE) {
            __asm__ __volatile__ (
                "xsaveq (%0)"
                : : "r"(buffer), "d"(0), "a"(0b111)
            );
        } else {
            __asm__ __volatile__ (
                "fxsaveq (%0)"
                : : "r"(buffer)
            );
        }
    }
    void RestoreFPUBlock(char* buffer) {
        if((uint64)buffer % 64 != 0)
            kprintf("misaligned: %i\n", (uint64)buffer % 64);
        if(g_ExtendedSSE) {
            __asm__ __volatile__ (
                "xrstorq (%0)"
                : : "r"(buffer), "d"(0), "a"(0b111)
            );
        } else {
            __asm__ __volatile__ (
                "fxrstorq (%0)"
                : : "r"(buffer)
            );
        }
    }

    void InitFPUBlock(char* buffer) {
        // mask all exceptions
        uint64 mxcsr = 0;
        mxcsr |= (1 << 12);
        mxcsr |= (1 << 11);
        mxcsr |= (1 << 10);
        mxcsr |= (1 << 9);
        mxcsr |= (1 << 8);
        mxcsr |= (1 << 7);
        ((uint32*)buffer)[6] = mxcsr;
    }

}