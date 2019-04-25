#pragma once

#include "Device.h"

class ZeroDevice : public Device
{
public:
    ZeroDevice(const char* name);

    uint64 Read(uint64 pos, void* buffer, uint64 bufferSize) override;
    uint64 Write(uint64 pos, void* buffer, uint64 bufferSize) override;
};