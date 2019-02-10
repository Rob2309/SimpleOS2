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
    
    KernelHeader = 0x80000001,
    KernelStack = 0x80000002,
    KernelImage = 0x80000003,
    MemoryMap = 0x80000004,
    Font = 0x80000005,
    TestImage = 0x80000006,
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
    uint64 size;
    uint8* buffer;
};

struct KernelHeader {
    uint64 memMapLength;
    uint64 memMapDescriptorSize;
    MemoryDescriptor* memMap;

    ModuleDescriptor kernelImage;
    ModuleDescriptor ramdiskImage;
    ModuleDescriptor helloWorldImage;
    ModuleDescriptor fontImage;

    uint32 screenWidth;
    uint32 screenHeight;
    uint32 screenScanlineWidth;
    uint32 screenBufferSize;
    uint32* screenBuffer; 

    void* stack;
    uint64 stackSize;
};