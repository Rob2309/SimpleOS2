#pragma once

#include "types.h"

namespace VFS {

    class File
    {
    public:
        virtual uint64 Read(uint64 pos, void* buffer, uint64 bufferSize) = 0;
        virtual uint64 Write(uint64 pos, const void* buffer, uint64 bufferSize) = 0;

        virtual uint64 GetSize() = 0;
    };

}