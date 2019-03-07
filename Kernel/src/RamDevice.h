#pragma once

#include "Device.h"

class RamDevice : public Device {
public:
    RamDevice(const char* name, void* buffer, uint64 size);

    void SetPos(uint64 pos) override;
    uint64 Read(void* buffer, uint64 bufferSize) override;
    void Write(void* buffer, uint64 bufferSize) override;

private:
    char* m_Buffer;
    uint64 m_Size;
    uint64 m_Pos;
};