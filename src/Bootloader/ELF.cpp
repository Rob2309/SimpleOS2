#include "ELF.h"

#include "conio.h"

uint64 GetELFSize(const uint8* image)
{
    ELFHeader* header = (ELFHeader*)image;

    uint64 size = 0;

    // The size needed for an elf image is just the largest address of the PT_LOAD segments
    ElfSegmentHeader* phList = (ElfSegmentHeader*)(image + header->phOffset);
    for(int s = 0; s < header->phEntryCount; s++) {
        ElfSegmentHeader* segment = &phList[s];
        if(segment->type == PT_LOAD) {
            uint64 offs = segment->virtualAddress + segment->virtualSize;
            if(offs > size)
                size = offs;
        }
    }

    return size;
}

static bool strcmp(const char* a, const char* b) {
    int index = 0;
    while(a[index] == b[index]) {
        if(a[index] == '\0')
            return true;
        index++;
    }
    return false;
}

static void PrintString(const char* a) {
    wchar_t buffer[2] = { 0 };

    int index = 0;
    while(a[index] != '\0') {
        buffer[0] = a[index];
        Console::Print(buffer);
        index++;
    }
}

uint64 GetELFTextAddr(const uint8* image, const uint8* processImg)
{
    ELFHeader* header = (ELFHeader*)image;

    ELFSectionHeader* shList = (ELFSectionHeader*)(image + header->shOffset);
    char* sectionNameTable = (char*)((uint64)image + shList[header->sectionNameStringTableIndex].fileOffset);
    for(int s = 0; s < header->shEntryCount; s++) {
        ELFSectionHeader* section = &shList[s];
        if(section->type == SHT_PROGBITS) {
            char* name = &sectionNameTable[section->nameOffset];
            if(strcmp(name, ".text")) {
                return (uint64)processImg + section->virtualAddress;
            }
        }
    }

    return 0;
}

bool PrepareELF(const uint8* diskImg, uint8* processImg, Elf64Addr* entryPoint)
{
    ELFHeader* header = (ELFHeader*)diskImg;

    ElfSegmentHeader* segList = (ElfSegmentHeader*)(diskImg + header->phOffset);
    for(int s = 0; s < header->phEntryCount; s++) {
        ElfSegmentHeader* segment = &segList[s];
        if(segment->type == PT_LOAD) {
            const uint8* src = diskImg + segment->dataOffset;
            uint8* dest = processImg + segment->virtualAddress;

            // Load the data into the segment
            for(uint64 i = 0; i < segment->dataSize; i++)
                dest[i] = src[i];
            
            // fill the rest of the segment with zeros
            for(uint64 i = segment->dataSize; i < segment->virtualSize; i++)
                dest[i] = 0;
        } else if(segment->type == PT_DYNAMIC) {
            Elf64Addr symtabAddr;

            Elf64Addr relaAddr = 0;
            Elf64XWord relaSize = 0;
            Elf64XWord relaEntrySize = sizeof(ElfRelA);
            Elf64XWord relaCount = 0;

            Elf64Addr pltrelAddr = 0;
            Elf64XWord pltrelSize = 0;
            Elf64XWord pltrelEntrySize = sizeof(ElfRelA);
            Elf64XWord pltrelCount = 0;

            Elf64Addr initArrayAddr = 0;
            Elf64XWord initArraySz = 0;

            ElfDynamicEntry* dyn = (ElfDynamicEntry*)(processImg + segment->virtualAddress);
            while(dyn->tag != DT_NULL) {
                if(dyn->tag == DT_SYMTAB) {
                    symtabAddr = dyn->value;
                }
                else if(dyn->tag == DT_JMPREL) {
                    pltrelAddr = dyn->value;
                }
                else if(dyn->tag == DT_PLTRELSZ) {
                    pltrelSize = dyn->value;
                }
                else if(dyn->tag == DT_RELAENT) {
                    pltrelEntrySize = dyn->value;
                }
                else if(dyn->tag == DT_RELA) {
                    relaAddr = dyn->value;
                }
                else if(dyn->tag == DT_RELASZ) {
                    relaSize = dyn->value;
                }
                else if(dyn->tag == DT_RELAENT) {
                    relaEntrySize = dyn->value;
                } else if(dyn->tag == DT_INIT_ARRAY) {
                    initArrayAddr = dyn->value;
                } else if(dyn->tag == DT_INIT_ARRAYSZ) {
                    initArraySz = dyn->value;
                }

                dyn++;
            }

            relaCount = relaSize / relaEntrySize;
            pltrelCount = pltrelSize / pltrelEntrySize;

            ElfRelA* relaList = (ElfRelA*)(processImg + relaAddr);
            ElfRelA* pltrelList = (ElfRelA*)(processImg + pltrelAddr);
            ElfSymbol* symList = (ElfSymbol*)(processImg + symtabAddr);

            for(unsigned int i = 0; i < relaCount; i++) {
                ElfRelA* rel = &relaList[i];

                unsigned int symIndex = rel->info >> 32;
                unsigned int relType = rel->info & 0xFFFFFFFF;
                Elf64Addr target = (Elf64Addr)processImg + rel->address;
                Elf64XWord addend = rel->addend;

                Elf64Addr symAddr = (Elf64Addr)processImg + symList[symIndex].value;
                Elf64XWord finalAddend = 0;
                int size = 0;

                switch(relType) {
                case R_NONE: 
                case R_COPY: continue;
                case R_64:      size = 8; finalAddend = symAddr + addend; break;
                case R_PC32:    size = 4; finalAddend = symAddr + addend - target; break;
                case R_GLOB_DAT:
                case R_JUMP_SLOT: size = 8; finalAddend = symAddr; break;
                case R_RELATIVE: size = 8; finalAddend = addend + (Elf64XWord)processImg; break;
                case R_32S:
                case R_32: size = 4; finalAddend = symAddr + addend; break;
                case R_PC64: size = 8; finalAddend = symAddr + addend - target; break;
                case R_SIZE32: size = 4; finalAddend = symList[symIndex].size + addend; break;
                case R_SIZE64: size = 8; finalAddend = symList[symIndex].size + addend; break;
                default: return false;
                }

                switch(size) {
                case 1: *(unsigned char*)target = finalAddend; break;
                case 2: *(unsigned short*)target = finalAddend; break;
                case 4: *(unsigned int*)target = finalAddend; break;
                case 8: *(unsigned long long*)target = finalAddend; break;
                default: break;
                }
            }

            for(unsigned int i = 0; i < pltrelCount; i++) {
                ElfRelA* rel = &pltrelList[i];

                unsigned int symIndex = rel->info >> 32;
                unsigned int relType = rel->info & 0xFFFFFFFF;
                Elf64Addr target = (Elf64Addr)processImg + rel->address;
                Elf64XWord addend = rel->addend;

                Elf64Addr symAddr = (Elf64Addr)processImg + symList[symIndex].value;
                Elf64XWord finalAddend = 0;
                int size = 0;

                switch(relType) {
                case R_NONE: 
                case R_COPY: continue;
                case R_64:      size = 8; finalAddend = symAddr + addend; break;
                case R_PC32:    size = 4; finalAddend = symAddr + addend - target; break;
                case R_GLOB_DAT:
                case R_JUMP_SLOT: size = 8; finalAddend = symAddr; break;
                case R_RELATIVE: size = 8; finalAddend = addend + (Elf64XWord)processImg; break;
                case R_32S:
                case R_32: size = 4; finalAddend = symAddr + addend; break;
                case R_PC64: size = 8; finalAddend = symAddr + addend - target; break;
                case R_SIZE32: size = 4; finalAddend = symList[symIndex].size + addend; break;
                case R_SIZE64: size = 8; finalAddend = symList[symIndex].size + addend; break;
                default: return false;
                }

                switch(size) {
                case 1: *(unsigned char*)target = finalAddend; break;
                case 2: *(unsigned short*)target = finalAddend; break;
                case 4: *(unsigned int*)target = finalAddend; break;
                case 8: *(unsigned long long*)target = finalAddend; break;
                default: break;
                }
            }

            if(initArrayAddr != 0)
            {
                initArrayAddr += (uint64)processImg;

                typedef void (**INITARRAY)();
                INITARRAY arr = (INITARRAY)initArrayAddr;

                for(int i = 0; i < initArraySz / 8; i++)
                    arr[i]();
            }
        }
    }

    *entryPoint = (Elf64Addr)processImg + header->entryPoint;
    return true;
}