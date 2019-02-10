#pragma once

#include "types.h"

namespace Paging
{
    void Init();

    uint64 MapHighPage(uint64 physPage);
    uint64 MapHighPages(uint64 physPage, uint64 numPages);
}