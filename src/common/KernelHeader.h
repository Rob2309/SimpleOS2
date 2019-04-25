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
    ModuleDescriptor kernelImage;   // The memory the Kernel occupies
    ModuleDescriptor ramdiskImage;  // The memory the ramdisk occupies

    uint32 screenWidth;             // Width of the screen in pixels
    uint32 screenHeight;            // Height of the screen in pixels
    uint32 screenScanlineWidth;     // Width of a screen scanline
    uint32 screenBufferPages;       // Number of pages the video buffer occupies
    uint32* screenBuffer;           // The video buffer
    bool screenColorsInverted;      // if true, the pixel format is 0xBBGGRR instead of 0xRRGGBB

    PhysicalMapSegment* physMapStart;   // The Physical memory map
    PhysicalMapSegment* physMapEnd;

    void* stack;                    // The stack the kernel is using on entry
    uint64 stackPages;              // The size of the stack in pages

    uint64* pageBuffer;             // Buffer of the initial paging structure
    uint64 pageBufferPages;
    uint64 highMemoryBase;          // The Virtual Memory Address associated with the lowest addressable physical address
};