#pragma once

#include "types.h"

enum class MemoryType : unsigned int {
    Reserved,
    LoaderCode,
    LoaderData,
    BootServicesCode,
    BootServicesData,
    RuntimeServicesCode,
    RuntimeServicesData,
    ConventionalMemory,
    UnusableMemory,
    ACPIReclaimMemory,
    ACPIMemoryNVS,
    MemoryMappedIO,
    MemoryMappedIOPortSpace,
    PalCode,
    MaxMemoryType
};

struct MemoryDescriptor {
    MemoryType type;
    unsigned int pad;
    unsigned long long physicalStart;
    unsigned long long virtualStart;
    unsigned long long numPages;
    enum : unsigned long long {
        ATTRIBUTE_RUNTIME = 0x8000000000000000
    } attributes;
};

struct ModuleDescriptor {
    enum {
        TYPE_UNKNOWN = 0,
        TYPE_ELF_IMAGE,
        TYPE_RAMDISK_IMAGE,
    } type;

    uint64 size;
    uint8* buffer;
};

struct KernelHeader {
    uint64 memMapLength;
    uint64 memMapDescriptorSize;
    MemoryDescriptor* memMap;

    uint64 numModules;
    ModuleDescriptor* modules;

    uint32 screenWidth;
    uint32 screenHeight;
    uint32 screenScanlineWidth;
    uint32 screenBufferSize;
    uint32* screenBuffer; 

    void* stack;
    uint64 stackSize;
};