#pragma once

#include "types.h"

struct ModuleDescriptor {
    uint64 size;
    uint8* buffer;
};

struct PhysicalMapSegment {
    uint64 base;
    uint64 numPages;
    uint8 map[];
};

struct KernelHeader {
    ModuleDescriptor kernelImage;
    ModuleDescriptor ramdiskImage;
    ModuleDescriptor helloWorldImage;
    ModuleDescriptor fontImage;

    uint32 screenWidth;
    uint32 screenHeight;
    uint32 screenScanlineWidth;
    uint32 screenBufferSize;
    uint32* screenBuffer; 

    PhysicalMapSegment* physMap;
    uint64 physMapSize;
    uint64 physMapSegments;

    void* stack;
    uint64 stackSize;

    uint64* pageBuffer;
    uint64 pageBufferSize;
    uint64 highMemoryBase;
};