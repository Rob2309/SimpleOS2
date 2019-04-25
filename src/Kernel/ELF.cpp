#include "ELF.h"

#include "MemoryManager.h"
#include "IDT.h"
#include "Scheduler.h"
#include "GDT.h"
#include "conio.h"

bool RunELF(const uint8* diskImg)
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

    IDT::Registers regs = { 0 };
    regs.cs = GDT::UserCode;
    regs.ds = GDT::UserData;
    regs.ss = GDT::UserData;
    regs.rflags = 0b000000000001000000000;
    regs.rip = entryPoint;
    regs.userrsp = stackBase + 16 * 4096;
    Scheduler::CreateProcess(pml4Entry, &regs);

    return true;
}