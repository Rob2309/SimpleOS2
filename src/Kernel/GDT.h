#pragma once

#include "types.h"

namespace GDT
{
    void Init();

    void SetKernelStack(uint64 rsp);
}