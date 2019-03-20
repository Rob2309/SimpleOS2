#pragma once

#include "types.h"
#include "KernelHeader.h"

namespace GDT
{
    void Init(KernelHeader* header);

    void SetKernelStack(uint64 rsp);
}