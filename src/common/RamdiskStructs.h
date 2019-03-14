#pragma once

#include "types.h"

struct RamdiskFile
{
    char name[50];

    uint64 size;
    uint64 dataOffset;
};

struct RamdiskHeader
{
    uint64 numFiles;
    RamdiskFile files[];
};