#pragma once

#include "KernelHeader.h"

namespace Paging
{
    void Init(KernelHeader* header);

    void* ConvertPtr(void* ptr);
}