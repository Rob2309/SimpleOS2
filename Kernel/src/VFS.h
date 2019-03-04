#pragma once

#include "types.h"

namespace VFS {

    enum FileType {
        FILETYPE_FILE,
        FILETYPE_DIRECTORY,
    };

    void Init();

    bool CreateFile(const char* path, const char* name, FileType type);

    constexpr uint64 OpenFileModeRead = 0x1;
    constexpr uint64 OpenFileModeWrite = 0x2;
    uint64 OpenFile(const char* path, uint64 mode);
    void CloseFile(uint64 file);

    uint64 ReadFile(uint64 file, void* buffer, uint64 bufferSize);
    void WriteFile(uint64 file, void* buffer, uint64 bufferSize);

}