#pragma once

#include "types.h"

struct File 
{
    uint64 size;
    uint8* data;
};

namespace Ramdisk {

    void Init(uint8* img);

    File GetFileData(const char* name);

}