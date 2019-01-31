#pragma once

#include "types.h"

struct MemoryDescriptor {
    unsigned int type;
    unsigned int pad;
    unsigned long long physicalStart;
    unsigned long long virtualStart;
    unsigned long long numPages;
    unsigned long long attributes;
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
    uint32* screenBuffer; 
};