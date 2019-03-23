#pragma once

#include "types.h"

namespace MSR
{
    constexpr uint32 RegEFER    = 0xC0000080;
    constexpr uint32 RegSTAR    = 0xC0000081;
    constexpr uint32 RegLSTAR   = 0xC0000082;
    constexpr uint32 RegCSTAR   = 0xC0000083;
    constexpr uint32 RegSFMASK  = 0xC0000084;

    constexpr uint32 RegFSBase          = 0xC0000100;
    constexpr uint32 RegGSBase          = 0xC0000101;
    constexpr uint32 RegKernelGSBase    = 0xC0000102;

    void Write(uint32 reg, uint64 val);
    uint64 Read(uint32 reg);
}