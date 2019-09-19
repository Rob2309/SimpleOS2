#include "ELF.h"
#include "ELFDefines.h"

#include "memory/MemoryManager.h"
#include "klib/memory.h"
#include "arch/GDT.h"
#include "klib/string.h"

#include "klib/stdio.h"

#include "ELFProgramInfo.h"

#include "init/Init.h"

static void Init() {
    ExecHandlerRegistry::RegisterHandler(new ELFExecHandler());
}
REGISTER_INIT_FUNC(Init, INIT_STAGE_EXECHANDLERS);

bool ELFExecHandler::CheckAndPrepare(uint8* buffer, uint64 bufferSize, uint64 pml4Entry, IDT::Registers* regs, int argc, const char* const* argv)
{
    constexpr uint64 stackBase = 0x1000;

    ELFHeader* header = (ELFHeader*)buffer;

    if(header->magic[0] != 0x7F || header->magic[1] != 'E' || header->magic[2] != 'L' || header->magic[3] != 'F')
        return false;

    ELFProgramInfo progInfo = { 0 };

    ElfSegmentHeader* segList = (ElfSegmentHeader*)(buffer + header->phOffset);
    for(int s = 0; s < header->phEntryCount; s++) {
        ElfSegmentHeader* segment = &segList[s];
        if(segment->type == PT_LOAD || segment->type == PT_TLS) {
            const uint8* src = buffer + segment->dataOffset;
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

            if(segment->type == PT_TLS) {
                progInfo.masterTLSAddress = (char*)segment->virtualAddress;
                progInfo.masterTLSSize = segment->virtualSize;
            }
        }
    }

    uint64 entryPoint = header->entryPoint;

    void* physStack = MemoryManager::AllocatePages(16);
    for(int i = 0; i < 16; i++)
        MemoryManager::MapProcessPage(pml4Entry, (void*)((uint64)physStack + i * 4096), (void*)(stackBase + i * 4096), false);

    char** userArgv = new char*[argc];

    char* virtStack = (char*)MemoryManager::PhysToKernelPtr(physStack) + 16 * 4096;
    int argSize = 0;

    for(int i = argc - 1; i >= 0; i--) {
        int l = kstrlen(argv[i]) + 1;
        virtStack -= l;
        argSize += l;
        kmemcpy(virtStack, argv[i], l);
        userArgv[i] = (char*)stackBase + 16 * 4096 - argSize;
    }

    for(int i = argc - 1; i >= 0; i--) {
        virtStack -= sizeof(char*);
        argSize += sizeof(char*);
        *(char**)virtStack = userArgv[i];
    }

    delete[] userArgv;

    progInfo.argc = argc;
    progInfo.argv = (char**)(stackBase + 16 * 4096 - argSize);
    kmemcpy(virtStack - sizeof(ELFProgramInfo), &progInfo, sizeof(ELFProgramInfo));

    kmemset(regs, 0, sizeof(IDT::Registers));
    regs->cs = GDT::UserCode;
    regs->ds = GDT::UserData;
    regs->ss = GDT::UserData;
    regs->rflags = 0b000000000001000000000;
    regs->rip = entryPoint;
    regs->userrsp = stackBase + 16 * 4096 - argSize - sizeof(ELFProgramInfo);
    regs->rdi = stackBase + 16 * 4096 - argSize - sizeof(ELFProgramInfo);

    return true;
}