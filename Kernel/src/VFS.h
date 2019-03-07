#pragma once

#include "types.h"

namespace VFS {

    void Init();

    bool CreateFile(const char* folder, const char* name);
    bool CreateFolder(const char* folder, const char* name);
    bool CreateDeviceFile(const char* folder, const char* name, uint64 devID);

    bool RemoveFile(const char* file);

    constexpr uint64 OpenFileModeRead = 0x1;
    constexpr uint64 OpenFileModeWrite = 0x2;
    constexpr uint64 OpenFileModeReadWrite = OpenFileModeRead | OpenFileModeWrite;
    uint64 OpenFile(const char* path, uint64 mode);
    void CloseFile(uint64 file);

    void SeekFile(uint64 file, uint64 pos);
    uint64 ReadFile(uint64 file, void* buffer, uint64 bufferSize);
    void WriteFile(uint64 file, void* buffer, uint64 bufferSize);

}