#pragma once

#include "types.h"

struct ELFProgramInfo {
    char* masterTLSAddress;
    uint64 masterTLSSize;
    int argc;
    char** argv;
};