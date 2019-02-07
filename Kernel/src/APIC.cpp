#include "APIC.h"

#include "conio.h"
#include "ISR.h"

#include "VirtualMemoryManager.h"
#include "PhysicalMemoryManager.h"

namespace APIC
{
    static uint64 g_APICBase;

    constexpr uint64 RegSpurious = 0xF0;
    constexpr uint64 RegError = 0x370;
    constexpr uint64 RegTimerDiv = 0x3E0;
    constexpr uint64 RegTimerMode = 0x320;
    constexpr uint64 RegTimerInitCount = 0x380;

    void Init()
    {
        uint32 eax, edx;
        __asm__ __volatile__ (
            "movl $0x1B, %%ecx;"
            "rdmsr"
            : "=a"(eax), "=d"(edx)
            :
            : "ecx"
        );

        g_APICBase = ((uint64)edx << 32) | (eax & 0xFFFFF000);
        printf("APIC Base: 0x%x\n", g_APICBase);

        VirtualMemoryManager::MapPage(g_APICBase, g_APICBase, true, true);

        *(uint32*)(g_APICBase + RegSpurious) = 0x100 | vectno_ISRApicSpurious;
        *(uint32*)(g_APICBase + RegError) = 0x10000 | vectno_ISRApicError;
    }
    void StartTimer(uint8 div, uint32 count, bool repeat)
    {
        switch(div)
        {
        case 128: *(uint32*)(g_APICBase + RegTimerDiv) = 0b1010; break;
        }

        *(uint32*)(g_APICBase + RegTimerMode) = (repeat ? 0x20000 : 0) | vectno_ISRApicTimer;
        *(uint32*)(g_APICBase + RegTimerInitCount) = count;
    }
}