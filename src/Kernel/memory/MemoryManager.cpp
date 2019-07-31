#include "MemoryManager.h"

#include "klib/stdio.h"
#include "klib/memory.h"
#include "ktl/FreeList.h"
#include "locks/StickyLock.h"
#include "arch/APIC.h"
#include "multicore/SMP.h"

#include "syscalls/SyscallDefine.h"
#include "scheduler/Scheduler.h"

#define PML_GET_NX(entry)           ((entry) & 0x8000000000000000)
#define PML_GET_ADDR(entry)         ((entry) & 0x000FFFFFFFFFF000)

#define PML_SET_NX                  0x8000000000000000
#define PML_SET_ADDR(addr)          (addr)

#define PML_GET_A(entry)            ((entry) & 0x20)
#define PML_GET_PCD(entry)          ((entry) & 0x10)
#define PML_GET_PWT(entry)          ((entry) & 0x8)
#define PML_GET_US(entry)           ((entry) & 0x4)
#define PML_GET_RW(entry)           ((entry) & 0x2)
#define PML_GET_P(entry)            ((entry) & 0x1)

#define PML_SET_A(a)                ((a) ? 0x20 : 0)
#define PML_SET_PCD(a)              ((a) ? 0x10 : 0)
#define PML_SET_PWT(a)              ((a) ? 0x8 : 0)
#define PML_SET_US(a)               ((a) ? 0x4 : 0)
#define PML_SET_RW(a)               ((a) ? 0x2 : 0)
#define PML_SET_P(a)                ((a) ? 0x1 : 0)

#define PML1_GET_G(entry)           ((entry) & 0x100)
#define PML1_GET_PAT(entry)         ((entry) & 0x80)
#define PML1_GET_D(entry)           ((entry) & 0x40)

#define PML1_SET_G(a)               ((a) ? 0x100 : 0)
#define PML1_SET_PAT(a)             ((a) ? 0x80 : 0)
#define PML1_SET_D(a)               ((a) ? 0x40 : 0)

#define GET_PML1_INDEX(addr)        (((addr) >> (12 + 0 * 9)) & 0x1FF)
#define GET_PML2_INDEX(addr)        (((addr) >> (12 + 1 * 9)) & 0x1FF)
#define GET_PML3_INDEX(addr)        (((addr) >> (12 + 2 * 9)) & 0x1FF)
#define GET_PML4_INDEX(addr)        (((addr) >> (12 + 3 * 9)) & 0x1FF)

namespace MemoryManager {

    static StickyLock g_Lock;
    static ktl::FreeList g_FreeList;

    static uint64 g_HighMemBase;

    static volatile uint64* g_CorePageTables[SMP::MaxCoreCount];
    static volatile uint64* g_KernelPages;
    static volatile uint64* g_KernelVPages;

    static StickyLock g_PageSyncLock;
    static void* g_PageSyncPage;
    static uint64 g_PageSyncFinishCount;

    static void ISR_PageSync(IDT::Registers* regs) {
        APIC::SignalEOI();
        InvalidatePage(g_PageSyncPage);
        __asm__ __volatile (
            "lock incq (%0)"
            : : "r"(&g_PageSyncFinishCount)
        );
    }

    void Init(KernelHeader* header)
    {
        g_FreeList = ktl::FreeList(header->physMapStart);
        g_HighMemBase = header->highMemoryBase;
        g_KernelPages = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(header->pageBuffer[511]));

        uint64 availableMemory = 0;
        for(const auto& a : g_FreeList) {
            availableMemory += a.size;
        }

        klog_info("MemoryManager", "Available memory: %i MB", availableMemory / 1024 / 1024);

        volatile uint64* heapPML3 = (volatile uint64*)PhysToKernelPtr(EarlyAllocatePages(1));
        for(int i = 0; i < 512; i++)
            heapPML3[i] = 0;
        g_KernelVPages = heapPML3;

        IDT::SetISR(ISRNumbers::IPIPagingSync, ISR_PageSync);

        klog_info("MemoryManager", "MemoryManager initialized");
    }

    void InitCore(uint64 coreID) {
        volatile uint64* pml4 = (uint64*)PhysToKernelPtr(EarlyAllocatePages(1));
        for(int i = 0; i < 512; i++)
            pml4[i] = 0;
        g_CorePageTables[coreID] = pml4;

        pml4[511] = PML_SET_ADDR((uint64)KernelToPhysPtr((uint64*)g_KernelPages)) | PML_SET_P(1) | PML_SET_RW(1);
        pml4[510] = PML_SET_ADDR((uint64)KernelToPhysPtr((uint64*)g_KernelVPages)) | PML_SET_P(1) | PML_SET_RW(1);

        __asm__ __volatile__ (
            "movq %0, %%cr3"
            : : "r"(KernelToPhysPtr((uint64*)pml4))
        );
    }

    void InvalidatePage(void* page) {
        __asm__ __volatile__ (
            "invlpg (%0)"
            : : "r"(page)
        );
    }

    static void* _AllocatePages(uint64 numPages) {
        void* p = g_FreeList.FindFree(numPages * 4096);
        if(p == nullptr) {
            return nullptr;
        }

        g_FreeList.MarkUsed(p, numPages * 4096);

        return KernelToPhysPtr(p);
    }
    static void _FreePages(void* pages, uint64 numPages) {
        g_FreeList.MarkFree(PhysToKernelPtr(pages), numPages * 4096);
    }

    void* EarlyAllocatePages(uint64 numPages) {
        g_Lock.Spinlock_Raw();
        void* res = _AllocatePages(numPages);
        g_Lock.Unlock_Raw();
        return res;
    }
    void EarlyFreePages(void* pages, uint64 numPages) {
        g_Lock.Spinlock_Raw();
        _FreePages(pages, numPages);
        g_Lock.Unlock_Raw();
    }

    void* AllocatePages(uint64 numPages)
    {
        g_Lock.Spinlock();
        void* res = _AllocatePages(numPages);
        g_Lock.Unlock();
        return res;
    }
    void FreePages(void* pages, uint64 numPages)
    {
        g_Lock.Spinlock();
        _FreePages(pages, numPages);
        g_Lock.Unlock();
    }

    void* PhysToKernelPtr(const void* ptr)
    {
        return (char*)ptr + g_HighMemBase;
    }

    void* KernelToPhysPtr(const void* ptr)
    {
        return (char*)ptr - g_HighMemBase;
    }

    static void FreeProcessPML1(volatile uint64* pml1)
    {
        for(int i = 0; i < 512; i++) {
            uint64 pml1Entry = pml1[i];
            if(PML_GET_P(pml1Entry)) {
                FreePages((void*)PML_GET_ADDR(pml1Entry), 1);
            }
        }
        FreePages(KernelToPhysPtr((uint64*)pml1), 1);
    }
    static void FreeProcessPML2(volatile uint64* pml2)
    {
        for(int i = 0; i < 512; i++) {
            uint64 pml2Entry = pml2[i];
            if(PML_GET_P(pml2Entry))
                FreeProcessPML1((uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml2Entry)));
        }
        FreePages(KernelToPhysPtr((uint64*)pml2), 1);
    }
    static void FreeProcessPML3(volatile uint64* pml3)
    {
        for(int i = 0; i < 512; i++) {
            uint64 pml3Entry = pml3[i];
            if(PML_GET_P(pml3Entry))
                FreeProcessPML2((uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml3Entry)));
        }
        FreePages(KernelToPhysPtr((uint64*)pml3), 1);
    }

    uint64 CreateProcessMap()
    {
        volatile uint64* pml3 = (volatile uint64*)PhysToKernelPtr(AllocatePages(1));
        for(int i = 0; i < 512; i++)
            pml3[i] = 0;

        uint64 pml4Entry = PML_SET_ADDR((uint64)KernelToPhysPtr((uint64*)pml3)) | PML_SET_P(1) | PML_SET_US(1) | PML_SET_RW(1);
        
        return pml4Entry;
    }
    uint64 ForkProcessMap()
    {
        volatile uint64* myPML4 = g_CorePageTables[SMP::GetLogicalCoreID()];

        uint64 newPML4Entry = CreateProcessMap();

        volatile uint64* pml3 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(myPML4[0]));

        for(uint64 i = 0; i < 512; i++) {
            uint64 pml3Entry = pml3[i];
            if(PML_GET_P(pml3Entry)) {
                volatile uint64* pml2 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml3Entry));
                for(uint64 j = 0; j < 512; j++) {
                    uint64 pml2Entry = pml2[j];
                    if(PML_GET_P(pml2Entry)) {
                        volatile uint64* pml1 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml2Entry));
                        for(uint64 k = 0; k < 512; k++) {
                            uint64 pml1Entry = pml1[k];
                            if(PML_GET_P(pml1Entry)) {
                                char* virt = (char*)((k << 12) | (j << 21) | (i << 30));

                                volatile uint64* dest = (uint64*)PhysToKernelPtr(AllocatePages(1));
                                volatile uint64* src = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml1Entry));
                                kmemcpy((uint64*)dest, (uint64*)src, 4096);
                                MapProcessPage(newPML4Entry, KernelToPhysPtr((uint64*)dest), virt, false);
                            }
                        }
                    }
                }
            }
        }

        return newPML4Entry;
    }

    void FreeProcessMap(uint64 pml4Entry)
    {
        volatile uint64* pml3 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml4Entry));
        FreeProcessPML3(pml3);
    }

    void SwitchProcessMap(uint64 pml4Entry)
    {
        volatile uint64* myPML4 = g_CorePageTables[SMP::GetLogicalCoreID()];

        myPML4[0] = pml4Entry;

        __asm__ __volatile__ (
            "movq %0, %%cr3"
            : : "r"(KernelToPhysPtr((uint64*)myPML4))
        );
    }

    void MapKernelPage(void* phys, void* virt)
    {
        volatile uint64* myPML4 = g_CorePageTables[SMP::GetLogicalCoreID()];

        uint64 pml4Index = GET_PML4_INDEX((uint64)virt);
        uint64 pml3Index = GET_PML3_INDEX((uint64)virt);
        uint64 pml2Index = GET_PML2_INDEX((uint64)virt);
        uint64 pml1Index = GET_PML1_INDEX((uint64)virt);

        g_Lock.Spinlock();

        uint64 pml4Entry = myPML4[pml4Index];
        volatile uint64* pml3 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml4Entry));

        uint64 pml3Entry = pml3[pml3Index];
        volatile uint64* pml2;
        if(!PML_GET_P(pml3Entry)) {
            pml2 = (uint64*)PhysToKernelPtr(_AllocatePages(1));
            for(int i = 0; i < 512; i++)
                pml2[i] = 0;
            pml3[pml3Index] = PML_SET_ADDR((uint64)KernelToPhysPtr((uint64*)pml2)) | PML_SET_P(1) | PML_SET_RW(1);
        } else {
            pml2 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml3Entry));
        }

        uint64 pml2Entry = pml2[pml2Index];
        volatile uint64* pml1;
        if(!PML_GET_P(pml2Entry)) {
            pml1 = (uint64*)PhysToKernelPtr(_AllocatePages(1));
            for(int i = 0; i < 512; i++)
                pml1[i] = 0;
            pml2[pml2Index] = PML_SET_ADDR((uint64)KernelToPhysPtr((uint64*)pml1)) | PML_SET_P(1) | PML_SET_RW(1);
        } else {
            pml1 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml2Entry));
        }

        pml1[pml1Index] = PML_SET_ADDR((uint64)phys) | PML_SET_P(1) | PML_SET_RW(1);

        g_Lock.Unlock();
    }
    void DisableChacheOnLargePage(void* virt) {
        volatile uint64* myPML4 = g_CorePageTables[SMP::GetLogicalCoreID()];

        uint64 pml4Index = GET_PML4_INDEX((uint64)virt);
        uint64 pml3Index = GET_PML3_INDEX((uint64)virt);
        uint64 pml2Index = GET_PML2_INDEX((uint64)virt);

        uint64 pml4Entry = myPML4[pml4Index];
        volatile uint64* pml3 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml4Entry));

        uint64 pml3Entry = pml3[pml3Index];
        volatile uint64* pml2 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml3Entry));

        pml2[pml2Index] |= PML_SET_PCD(1);

        __asm__ __volatile__ (
            "invlpg (%0);"
            "wbinvd"
            : : "r" (virt)
        );
    }

    void MapProcessPage(uint64 pml4Entry, void* phys, void* virt, bool invalidate)
    {
        uint64 pml3Index = GET_PML3_INDEX((uint64)virt);
        uint64 pml2Index = GET_PML2_INDEX((uint64)virt);
        uint64 pml1Index = GET_PML1_INDEX((uint64)virt);

        volatile uint64* pml3 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml4Entry));

        uint64 pml3Entry = pml3[pml3Index];
        volatile uint64* pml2;
        if(!PML_GET_P(pml3Entry)) {
            pml2 = (uint64*)PhysToKernelPtr(AllocatePages());
            for(int i = 0; i < 512; i++)
                pml2[i] = 0;
            pml3[pml3Index] = PML_SET_ADDR((uint64)KernelToPhysPtr((uint64*)pml2)) | PML_SET_P(1) | PML_SET_RW(1) | PML_SET_US(1);
        } else {
            pml2 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml3Entry));
        }

        uint64 pml2Entry = pml2[pml2Index];
        volatile uint64* pml1;
        if(!PML_GET_P(pml2Entry)) {
            pml1 = (uint64*)PhysToKernelPtr(AllocatePages());
            for(int i = 0; i < 512; i++)
                pml1[i] = 0;
            pml2[pml2Index] = PML_SET_ADDR((uint64)KernelToPhysPtr((uint64*)pml1)) | PML_SET_P(1) | PML_SET_RW(1) | PML_SET_US(1);
        } else {
            pml1 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml2Entry));
        }

        pml1[pml1Index] = PML_SET_ADDR((uint64)phys) | PML_SET_P(1) | PML_SET_RW(1) | PML_SET_US(1);

        if(invalidate) {
            __asm__ __volatile__ (
                "invlpg (%0)"
                : : "r"(virt)
            );
        }
    }
    void* MapProcessPage(uint64 pml4Entry, void* virt, bool invalidate)
    {
        uint64 pml3Index = GET_PML3_INDEX((uint64)virt);
        uint64 pml2Index = GET_PML2_INDEX((uint64)virt);
        uint64 pml1Index = GET_PML1_INDEX((uint64)virt);

        volatile uint64* pml3 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml4Entry));

        uint64 pml3Entry = pml3[pml3Index];
        volatile uint64* pml2;
        if(!PML_GET_P(pml3Entry)) {
            pml2 = (uint64*)PhysToKernelPtr(AllocatePages());
            for(int i = 0; i < 512; i++)
                pml2[i] = 0;
            pml3[pml3Index] = PML_SET_ADDR((uint64)KernelToPhysPtr((uint64*)pml2)) | PML_SET_P(1) | PML_SET_RW(1) | PML_SET_US(1);
        } else {
            pml2 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml3Entry));
        }

        uint64 pml2Entry = pml2[pml2Index];
        volatile uint64* pml1;
        if(!PML_GET_P(pml2Entry)) {
            pml1 = (uint64*)PhysToKernelPtr(AllocatePages());
            for(int i = 0; i < 512; i++)
                pml1[i] = 0;
            pml2[pml2Index] = PML_SET_ADDR((uint64)KernelToPhysPtr((uint64*)pml1)) | PML_SET_P(1) | PML_SET_RW(1) | PML_SET_US(1);
        } else {
            pml1 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml2Entry));
        }

        uint64 pml1Entry = pml1[pml1Index];
        if(PML_GET_P(pml1Entry))
            return PhysToKernelPtr((void*)PML_GET_ADDR(pml1Entry));
            
        void* phys = MemoryManager::AllocatePages(1);
        pml1[pml1Index] = PML_SET_ADDR((uint64)phys) | PML_SET_P(1) | PML_SET_RW(1) | PML_SET_US(1);

        if(invalidate) {
            __asm__ __volatile__ (
                "invlpg (%0)"
                : : "r"(virt)
            );
        }

        return PhysToKernelPtr(phys);
    }
    void MapProcessPage(void* virt) {
        volatile uint64* myPML4 = g_CorePageTables[SMP::GetLogicalCoreID()];
        MapProcessPage(myPML4[0], virt, true);
    }
    SYSCALL_DEFINE(syscall_alloc) {
        char* pageBase = (char*)arg1;
        if(!MemoryManager::IsUserPtr(pageBase))
            Scheduler::ThreadKillProcess("InvalidUserPointer");
        for(uint64 i = 0; i < arg2; i++) {
            MemoryManager::MapProcessPage(pageBase + i * 4096);
        }
        return 0;
    }
    void UnmapProcessPage(uint64 pml4Entry, void* virt)
    {
        uint64 pml3Index = GET_PML3_INDEX((uint64)virt);
        uint64 pml2Index = GET_PML2_INDEX((uint64)virt);
        uint64 pml1Index = GET_PML1_INDEX((uint64)virt);

        volatile uint64* pml3 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml4Entry));

        uint64 pml3Entry = pml3[pml3Index];
        volatile uint64* pml2 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml3Entry));

        uint64 pml2Entry = pml2[pml2Index];
        volatile uint64* pml1 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml2Entry));

        pml1[pml1Index] = 0;

        __asm__ __volatile__ (
            "invlpg (%0)"
            : : "r"(virt)
        );

        g_PageSyncLock.Spinlock();
        g_PageSyncFinishCount = 1;
        g_PageSyncPage = virt;
        APIC::SendIPI(APIC::IPI_TARGET_ALL_BUT_SELF, 0, ISRNumbers::IPIPagingSync);
        while(g_PageSyncFinishCount < SMP::GetCoreCount()) ;
        g_PageSyncLock.Unlock();
    }
    void UnmapProcessPage(void* virt) {
        volatile uint64* myPML4 = g_CorePageTables[SMP::GetLogicalCoreID()];
        UnmapProcessPage(myPML4[0], virt);
    }
    SYSCALL_DEFINE(syscall_free) {
        char* pageBase = (char*)arg1;
        if(!MemoryManager::IsUserPtr(pageBase))
            Scheduler::ThreadKillProcess("InvalidUserPointer");
        for(uint64 i = 0; i < arg2; i++) {
            MemoryManager::UnmapProcessPage(pageBase + i * 4096);
        }
        return 0;
    }

    void* UserToKernelPtr(const void* virt)
    {
        volatile uint64* myPML4 = g_CorePageTables[SMP::GetLogicalCoreID()];
        uint64 pml4Entry = myPML4[0];

        uint64 pml3Index = GET_PML3_INDEX((uint64)virt);
        uint64 pml2Index = GET_PML2_INDEX((uint64)virt);
        uint64 pml1Index = GET_PML1_INDEX((uint64)virt);
        uint64 offs = (uint64)virt & 0xFFF;

        volatile uint64* pml3 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml4Entry));

        uint64 pml3Entry = pml3[pml3Index];
        volatile uint64* pml2;
        if(!PML_GET_P(pml3Entry)) {
            return nullptr;
        } else {
            pml2 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml3Entry));
        }

        uint64 pml2Entry = pml2[pml2Index];
        volatile uint64* pml1;
        if(!PML_GET_P(pml2Entry)) {
            return nullptr;
        } else {
            pml1 = (uint64*)PhysToKernelPtr((void*)PML_GET_ADDR(pml2Entry));
        }

        uint64 pml1Entry = pml1[pml1Index];
        
        uint64 phys = PML_GET_ADDR(pml1Entry) | offs;
        return PhysToKernelPtr((void*)phys);
    }

    bool IsUserPtr(const void* ptr) {
        uint64 pml4Index = GET_PML4_INDEX((uint64)ptr);
        return pml4Index == 0;
    }

}