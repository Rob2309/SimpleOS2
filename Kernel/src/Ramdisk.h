#pragma once

#include "types.h"

struct File 
{
    uint64 size;
    uint8* data;
};

namespace Ramdisk {

    void Init(const char* dev);

    File GetFileData(const char* name);

}