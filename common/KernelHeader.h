#pragma once

typedef struct {
    unsigned long long start;
    unsigned long long pages;
} MemoryDescriptor;

typedef struct {
    void (__attribute__((ms_abi)) *printf)(const char* format, ...);

    unsigned long long memMapLength;
    MemoryDescriptor* memMap;
} KernelHeader;