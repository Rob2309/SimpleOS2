#pragma once

#include "types.h"

struct ModuleDescriptor {
    uint64 numPages;
    uint8* buffer;
};

struct PhysicalMapSegment {
    uint64 base;
    uint64 numPages;

    PhysicalMapSegment* next;
    PhysicalMapSegment* prev;
};

struct KernelHeader {
    ModuleDescriptor kernelImage;
    ModuleDescriptor ramdiskImage;

    uint32 screenWidth;
    uint32 screenHeight;
    uint32 screenScanlineWidth;
    uint32 screenBufferPages;
    uint32* screenBuffer;
    bool screenColorsInverted;

    PhysicalMapSegment* physMapStart;
    PhysicalMapSegment* physMapEnd;

    void* stack;
    uint64 stackPages;

    uint64* pageBuffer;
    uint64 pageBufferPages;
    uint64 highMemoryBase;
};