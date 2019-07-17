#pragma once

#include "types.h"

struct ELFProgramInfo;

struct ELFThread {
    ELFThread* selfPtr;
    void* allocPtr;
    ELFProgramInfo* progInfo;
};

struct ELFProgramInfo {
    char* masterTLSAddress;
    uint64 masterTLSSize;
};