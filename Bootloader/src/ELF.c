#include "ELF.h"

#include "util.h"

unsigned int GetELFSize(const char* image)
{
	ELFHeader* header = (ELFHeader*)image;

	unsigned int maxOffset = 0;

	ElfSegmentHeader* phList = (ElfSegmentHeader*)(image + header->phOffset);
	for(int s = 0; s < header->phEntryCount; s++)
	{
		ElfSegmentHeader* ph = &phList[s];
		if(ph->type == PT_LOAD)
		{
			unsigned int offs = ph->virtualAddress + ph->virtualSize;
			if(offs > maxOffset)
				maxOffset = offs;
		}
	}

	return maxOffset;
}

bool IsSupportedELF(const char* image)
{
    ELFHeader* header = (ELFHeader*)image;

	if((header->magic[0] != 0x7F) || (header->magic[1] != 0x45) || (header->magic[2] != 0x4C) || (header->magic[3] != 0x46))
		return 0;
    if(header->elfBits != ELFCLASS64)
        return 0;
    if(header->machineType != EMT_IA64 && header->machineType != EMT_X86_64)
        return 0;

	return 1;
}

bool PrepareELF(const char* baseImg, char* loadBuffer, Elf64Addr* entryPoint)
{
    ELFHeader* header = (ELFHeader*)baseImg;

    ElfSegmentHeader* segList = (ElfSegmentHeader*)(baseImg + header->phOffset);
    for(int s = 0; s < header->phEntryCount; s++)
    {
        ElfSegmentHeader* seg = &segList[s];
        if(seg->type == PT_LOAD)
        {
            char* src = baseImg + seg->dataOffset;
            char* dest = loadBuffer + seg->virtualAddress;

            for(int i = 0; i < seg->dataSize; i++)
                dest[i] = src[i];
            for(int i = seg->dataSize; i < seg->virtualSize; i++)
                dest[i] = 0;
        } 
        else if(seg->type == PT_DYNAMIC)
        {
            Elf64Addr strtabAddr;
            Elf64Addr symtabAddr;
            Elf64Addr relAddr;
            Elf64XWord relSize;
            Elf64XWord relEntrySize = 24;
            Elf64XWord relCount;

            ElfDynamicEntry* dyn = (ElfDynamicEntry*)(loadBuffer + seg->virtualAddress);
            while(dyn->tag != DT_NULL)
            {
                if(dyn->tag == DT_STRTAB)
                    strtabAddr = dyn->value;
                else if(dyn->tag == DT_SYMTAB)
                    symtabAddr = dyn->value;
                else if(dyn->tag == DT_JMPREL)
                    relAddr = dyn->value;
                else if(dyn->tag == DT_PLTRELSZ)
                    relSize = dyn->value;
                else if(dyn->tag == DT_RELAENT)
                    relEntrySize = dyn->value;

                dyn++;
            }

            relCount = relSize / relEntrySize;

            ElfRelA* relList = (ElfRelA*)(loadBuffer + relAddr);
            ElfSymbol* symList = (ElfSymbol*)(loadBuffer + symtabAddr);
            const char* strList = (const char*)(loadBuffer + strtabAddr);

            for(unsigned int i = 0; i < relCount; i++) {
                ElfRelA* rel = &relList[i];

                unsigned int symIndex = rel->info >> 32;
                unsigned int relType = rel->info & 0xFFFFFFFF;
                Elf64Addr target = (Elf64Addr)loadBuffer + rel->address;
                Elf64XWord addend = rel->addend;

                /*
                    B: Image load address
                    G: offset into GOT
                    GOT: GOT address
                    L: PLT address
                    P: address of reloc target
                    S: Symbol address
                    Z: Symbol size
                    A: Addend
                */

                printf("Symbol: %s\n", &strList[symList[symIndex].symbolNameOffset]);

                Elf64Addr symAddr = (Elf64Addr)loadBuffer + symList[symIndex].value;
                Elf64XWord finalAddend = 0;
                int size = 0;

                switch(relType) {
                case R_NONE: 
                case R_COPY: continue;
                case R_64:      size = 8; finalAddend = symAddr + addend; break;
                case R_PC32:    size = 4; finalAddend = symAddr + addend - target; break;
                case R_GLOB_DAT:
                case R_JUMP_SLOT: size = 8; finalAddend = symAddr; break;
                case R_RELATIVE: size = 8; finalAddend = addend + (Elf64XWord)loadBuffer; break;
                case R_32S:
                case R_32: size = 4; finalAddend = symAddr + addend; break;
                case R_PC64: size = 8; finalAddend = symAddr + addend - target; break;
                case R_SIZE32: size = 4; finalAddend = symList[symIndex].size + addend; break;
                case R_SIZE64: size = 8; finalAddend = symList[symIndex].size + addend; break;
                default: return 0;
                }

                switch(size) {
                case 1: *(unsigned char*)target = finalAddend; break;
                case 2: *(unsigned short*)target = finalAddend; break;
                case 4: *(unsigned int*)target = finalAddend; break;
                case 8: *(unsigned long long*)target = finalAddend; break;
                default: break;
                }
            }
        }
    }

    *entryPoint = (Elf64Addr)loadBuffer + header->entryPoint;

    return 1;
}