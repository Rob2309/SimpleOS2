#pragma once

#include "DeviceDriver.h"

class PseudoDeviceDriver : public CharDeviceDriver {
public:
    static constexpr uint64 DeviceZero = 0;

public:
    uint64 Read(uint64 subID, uint64 pos, void* buffer, uint64 bufferSize) override;
    uint64 Write(uint64 subID, uint64 pos, const void* buffer, uint64 bufferSize) override;
};