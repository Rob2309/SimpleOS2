#include "ELF.h"

#include "memory/MemoryManager.h"
#include "interrupts/IDT.h"
#include "Scheduler.h"
#include "arch/GDT.h"

bool PrepareELF(const uint8* diskImg, uint64& outPML4Entry, IDT::Registers& outRegs)
{
    constexpr uint64 stackBase = 0x1000;

    ELFHeader* header = (ELFHeader*)diskImg;

    uint64 pml4Entry = MemoryManager::CreateProcessMap();

    ElfSegmentHeader* segList = (ElfSegmentHeader*)(diskImg + header->phOffset);
    for(int s = 0; s < header->phEntryCount; s++) {
        ElfSegmentHeader* segment = &segList[s];
        if(segment->type == PT_LOAD) {
            const uint8* src = diskImg + segment->dataOffset;
            uint8* dest = (uint8*)segment->virtualAddress;

            uint64 lastPage = 0;
            uint8* lastPageBuffer = nullptr;
            for(uint64 i = 0; i < segment->dataSize; i++) {
                uint64 addr = (uint64)dest + i;
                uint64 page = addr & 0xFFFFFFFFFFFFF000;
                uint64 offs = addr & 0xFFF;
                if(page != lastPage) {
                    lastPage = page;
                    lastPageBuffer = (uint8*)MemoryManager::MapProcessPage(pml4Entry, (void*)page, false);
                }

                lastPageBuffer[offs] = src[i];
            }
            for(uint64 i = segment->dataSize; i < segment->virtualSize; i++) {
                uint64 addr = (uint64)dest + i;
                uint64 page = addr & 0xFFFFFFFFFFFFF000;
                uint64 offs = addr & 0xFFF;
                if(page != lastPage) {
                    lastPage = page;
                    lastPageBuffer = (uint8*)MemoryManager::MapProcessPage(pml4Entry, (void*)page, false);
                }

                lastPageBuffer[offs] = 0;
            }
        }
    }

    uint64 entryPoint = header->entryPoint;

    void* physStack = MemoryManager::AllocatePages(16);
    for(int i = 0; i < 16; i++)
        MemoryManager::MapProcessPage(pml4Entry, (void*)((uint64)physStack + i * 4096), (void*)(stackBase + i * 4096), false);

    outRegs = { 0 };
    outRegs.cs = GDT::UserCode;
    outRegs.ds = GDT::UserData;
    outRegs.ss = GDT::UserData;
    outRegs.rflags = 0b000000000001000000000;
    outRegs.rip = entryPoint;
    outRegs.userrsp = stackBase + 16 * 4096;

    outPML4Entry = pml4Entry;
    return true;
}

bool RunELF(const uint8* diskImg, User* user)
{
    uint64 pml4Entry;
    IDT::Registers regs;
    if(!PrepareELF(diskImg, pml4Entry, regs))
        return false;
    Scheduler::CreateUserThread(pml4Entry, &regs, user);
    return true;
}