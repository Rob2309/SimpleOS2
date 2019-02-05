#include "VirtualMemoryManager.h"

#include "PhysicalMemoryManager.h"

namespace PhysicalMemoryManager
{
    extern void* GetPhysicalMapPointer();
    extern uint64 GetPhysicalMapPageCount();
}

namespace VirtualMemoryManager
{
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

    #define GET_PML1_INDEX(addr)        (((addr) & 0x1FF000) >> 12)
    #define GET_PML2_INDEX(addr)        (((addr) & 0x3FE00000) >> 21)
    #define GET_PML3_INDEX(addr)        (((addr) & 0x7FC0000000) >> 30)
    #define GET_PML4_INDEX(addr)        (((addr) & 0xFF8000000000) >> 39)

    static uint64* g_PML4 = nullptr;

    static uint64* g_TempPage = nullptr;
    static uint64* g_TempPageTable = nullptr;

    static void EarlyIdentityMap(uint64 physAddr)
    {
        uint64 pml4Entry = g_PML4[GET_PML4_INDEX(physAddr)];
        uint64* pml3 = (uint64*)PML_GET_ADDR(pml4Entry);
        if(!PML_GET_P(pml4Entry)) {
            pml3 = (uint64*)PhysicalMemoryManager::AllocateCleanPage();
            g_PML4[GET_PML4_INDEX(physAddr)] = PML_SET_ADDR((uint64)pml3) | PML_SET_P(1) | PML_SET_RW(1) | PML_SET_US(1);
        }

        uint64 pml3Entry = pml3[GET_PML3_INDEX(physAddr)];
        uint64* pml2 = (uint64*)PML_GET_ADDR(pml3Entry);
        if(!PML_GET_P(pml3Entry)) {
            pml2 = (uint64*)PhysicalMemoryManager::AllocateCleanPage();
            pml3[GET_PML3_INDEX(physAddr)] = PML_SET_ADDR((uint64)pml2) | PML_SET_P(1) | PML_SET_RW(1) | PML_SET_US(1);
        }

        uint64 pml2Entry = pml2[GET_PML2_INDEX(physAddr)];
        uint64* pml1 = (uint64*)PML_GET_ADDR(pml2Entry);
        if(!PML_GET_P(pml2Entry)) {
            pml1 = (uint64*)PhysicalMemoryManager::AllocateCleanPage();
            pml2[GET_PML2_INDEX(physAddr)] = PML_SET_ADDR((uint64)pml1) | PML_SET_P(1) | PML_SET_RW(1) | PML_SET_US(1);
        }

        pml1[GET_PML1_INDEX(physAddr)] = PML_SET_ADDR(physAddr) | PML_SET_P(1) | PML_SET_RW(1) | PML_SET_US(1);
    }

    void Init(KernelHeader* header)
    {
        g_PML4 = (uint64*)PhysicalMemoryManager::AllocateCleanPage();
        EarlyIdentityMap((uint64)g_PML4);

        g_TempPage = (uint64*)PhysicalMemoryManager::AllocateCleanPage();
        EarlyIdentityMap((uint64)g_TempPage);

        uint64* tempPagePML3 = (uint64*)PML_GET_ADDR(g_PML4[GET_PML4_INDEX((uint64)g_TempPage)]);
        uint64* tempPagePML2 = (uint64*)PML_GET_ADDR(tempPagePML3[GET_PML3_INDEX((uint64)g_TempPage)]);
        uint64* tempPagePML1 = (uint64*)PML_GET_ADDR(tempPagePML2[GET_PML2_INDEX((uint64)g_TempPage)]);
        EarlyIdentityMap((uint64)tempPagePML1);
        g_TempPageTable = tempPagePML1;

        // identity map physical memory map
        uint64 physMap = (uint64)PhysicalMemoryManager::GetPhysicalMapPointer();
        uint64 physMapPages = PhysicalMemoryManager::GetPhysicalMapPageCount();
        for(uint64 i = 0; i < physMapPages; i++)
            EarlyIdentityMap(physMap + i * 4096);

        // identity map header
        uint64 headerPageCount = (sizeof(KernelHeader) + 4095) / 4096;
        for(uint64 i = 0; i < headerPageCount; i++)
            EarlyIdentityMap((uint64)header + i * 4096);

        // identity map module descriptors
        uint64 moduleDescPageCount = (header->numModules * sizeof(ModuleDescriptor) + 4095) / 4096;
        for(uint64 i = 0; i < moduleDescPageCount; i++)
            EarlyIdentityMap((uint64)(header->modules) + i * 4096);

        // identity map module buffers
        for(int i = 0; i < header->numModules; i++) {
            uint64 modulePageCount = (header->modules[i].size + 4095) / 4096;
            for(uint64 j = 0; j < modulePageCount; j++)
                EarlyIdentityMap((uint64)(header->modules[i].buffer) + j * 4096);
        }

        // identity map header memory map
        uint64 headerMemMapPages = (header->memMapLength * header->memMapDescriptorSize + 4095) / 4096;
        for(int i = 0; i < headerMemMapPages; i++)
            EarlyIdentityMap((uint64)(header->memMap) + i * 4096);

        // Identity map stack
        uint64 stackPages = (header->stackSize + 4095) / 4096;
        for(int i = 0; i < stackPages; i++)
            EarlyIdentityMap((uint64)(header->stack) + i * 4096);

        // load new PML4
        __asm__ __volatile__ (
            "movq %%eax, %%cr3;"
            : : "a" (g_PML4)
        );
    }

    uint64 MapPage(uint64 physPage, bool disableCache);
    void MapPage(uint64 physPage, uint64 virtPage, bool disableCache)
    {
        
    }
    void UnmapPage(uint64 virtPage)
    {

    }
}