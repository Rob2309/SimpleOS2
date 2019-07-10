#pragma once

#include "types.h"
#include "interrupts/IDT.h"
#include "user/User.h"

typedef unsigned long long Elf64Addr;
typedef unsigned long long Elf64Offs;
typedef unsigned short Elf64Half;
typedef unsigned int Elf64Word;
typedef signed int Elf64SWord;
typedef unsigned long long Elf64XWord;
typedef signed long long Elf64SXWord;
typedef unsigned char Elf64Byte;
typedef unsigned short Elf64Section;

/**
 * Creates a user process from the given elf executable image
 **/
bool RunELF(const uint8* diskImg, User* user);
bool PrepareELF(const uint8* diskImg, uint64& pml4Entry, IDT::Registers& regs);

struct ELFHeader {
    Elf64Byte magic[4];
    Elf64Byte elfBits;
    Elf64Byte endianness;
    Elf64Byte elfVersion;
    Elf64Byte abi;
    Elf64Byte padding[8];
    Elf64Half objectType;
    Elf64Half machineType;
    Elf64Word elfXVersion;
    Elf64Addr entryPoint;
    Elf64Offs phOffset;
    Elf64Offs shOffset;
    Elf64Word flags;
    Elf64Half headerSize;
    Elf64Half phEntrySize;
    Elf64Half phEntryCount;
    Elf64Half shEntrySize;
    Elf64Half shEntryCount;
    Elf64Half sectionNameStringTableIndex;
};

#define ELFCLASS32 1
#define ELFCLASS64 2

#define ELFDATALSB 1
#define ELFDATAMSB 2

#define ELFOSABI_SYSV 0
#define ELFOSABI_HPUX 1
#define ELFOSABI_STANDALONE 255

#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4

#define EMT_X86_64 0x3E
#define EMT_IA64 0x32

struct ELFSectionHeader {
    Elf64Word nameOffset;
    Elf64Word type;
    Elf64XWord flags;
    Elf64Addr virtualAddress;
    Elf64Offs fileOffset;
    Elf64XWord size;
    Elf64Word link;
    Elf64Word info;
    Elf64XWord alignment;
    Elf64XWord entrySize;
};

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_SHLIB 10
#define SHT_DYNSYM 11

#define SHF_WRITE 0x01
#define SHF_ALLOC 0x02
#define SHF_EXECINSTR 0x04

struct ElfSegmentHeader {
    Elf64Word type;
    Elf64Word flags;
    Elf64Offs dataOffset;
    Elf64Addr virtualAddress;
    Elf64XWord unused;
    Elf64XWord dataSize;
    Elf64XWord virtualSize;
    Elf64XWord alignment;
};

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4

#define PF_EXEC 1
#define PF_WRITABLE 2
#define PF_READABLE 4

struct ElfDynamicEntry {
    Elf64SXWord tag;
    Elf64XWord value;
};

#define DT_NULL 0
#define DT_NEEDED 1
#define DT_PLTRELSZ 2
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
#define DT_STRSZ 10
#define DT_SYMENT 11
#define DT_INIT 12
#define DT_FINI 13
#define DT_SONAME 14
#define DT_RPATH 15
#define DT_REL 17
#define DT_RELSZ 18
#define DT_RELENT 19
#define DT_JMPREL 23
#define DT_INIT_ARRAY 0x19
#define DT_INIT_ARRAYSZ 0x1B

struct ElfSymbol {
    Elf64Word symbolNameOffset;
    Elf64Byte info;
    Elf64Byte reserved;
    Elf64Half sectionTableIndex;
    Elf64Addr value;
    Elf64XWord size;
};

#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4

struct ElfRelA {
    Elf64Addr address;
    Elf64XWord info;
    Elf64SXWord addend;
};

#define R_NONE 0
#define R_64 1
#define R_PC32 2
#define R_GOT32 3
#define R_PLT32 4
#define R_COPY 5
#define R_GLOB_DAT 6
#define R_JUMP_SLOT 7
#define R_RELATIVE 8
#define R_GOTPCREL 9
#define R_32 10
#define R_32S 11
#define R_16 12
#define R_PC16 13
#define R_8 14
#define R_PC8 15
#define R_DTPMOD64 16
#define R_DTPOFF64 17
#define R_TPOFF64 18
#define R_TLSGD 19
#define R_TLSLD 20
#define R_DTPOFF32 21
#define R_GOTTPOFF 22
#define R_TPOFF32 23
#define R_PC64 24
#define R_GOTOFF64 25
#define R_GOTPC32 26
#define R_SIZE32 32
#define R_SIZE64 33
#define R_GOTPC32_TLSDESC 34
#define R_TLSDESC_CALL 35
#define R_TLSDESC 36
#define R_IRELATIVE 37