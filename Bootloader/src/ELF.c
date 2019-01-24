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

bool PrepareELF(const char* baseImg, char* loadBuffer)
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
            Elf64XWord relEntrySize;
            Elf64XWord relCount;

            ElfDynamicEntry* dyn = (ElfDynamicEntry*)(loadBuffer + seg->virtualAddress);
            while(dyn->tag != DT_NULL)
            {
                if(dyn->tag == DT_STRTAB)
                    strtabAddr = dyn->value;
                else if(dyn->tag == DT_SYMTAB)
                    symtabAddr = dyn->value;
                else if(dyn->tag == DT_REL)
                    relAddr = dyn->value;
                else if(dyn->tag == DT_RELSZ)
                    relSize = dyn->value;
                else if(dyn->tag == DT_RELENT)
                    relEntrySize = dyn->value;

                dyn++;
            }

            relCount = relSize / relEntrySize;

            ElfRel* relList = (ElfRel*)(loadBuffer + relAddr);
            ElfSymbol* symList = (ElfSymbol*)(loadBuffer + symtabAddr);
            const char* strList = (const char*)(loadBuffer + strtabAddr);

            for(unsigned int i = 0; i < relCount; i++) {
                ElfRel* rel = &relList[i];

                unsigned int symIndex = rel->info >> 32;
                unsigned int relType = rel->info & 0xFFFFFFFF;
                Elf64Addr target = (Elf64Addr)loadBuffer + symList[symIndex].value;

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
               
            }
        }
    }
}