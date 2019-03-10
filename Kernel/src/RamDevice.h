#pragma once

#include "Device.h"

class RamDevice : public Device {
public:
    RamDevice(const char* name, void* buffer, uint64 size);

    uint64 Read(uint64 pos, void* buffer, uint64 bufferSize) override;
    void Write(uint64 pos, void* buffer, uint64 bufferSize) override;

private:
    char* m_Buffer;
    uint64 m_Size;
};