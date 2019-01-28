#pragma once

typedef struct {
    unsigned int type;
    unsigned int pad;
    unsigned long long physicalStart;
    unsigned long long virtualStart;
    unsigned long long numPages;
    unsigned long long attributes;
} MemoryDescriptor;

typedef struct {
    void (__attribute__((ms_abi)) *printf)(const char* format, ...);

    unsigned long long memMapLength;
    unsigned long long memMapDescriptorSize;
    MemoryDescriptor* memMap;
} KernelHeader;