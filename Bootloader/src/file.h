#pragma once

#include "types.h"

namespace FileIO {

    struct FileData {
        uint64 size;
        uint8* data;
    };

    FileData ReadFile(const wchar_t* path);

}