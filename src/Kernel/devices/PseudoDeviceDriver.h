#pragma once

#include "DeviceDriver.h"

/**
 * This driver implements many of the kernel-provided pseudo devices.
 **/
class PseudoDeviceDriver : public CharDeviceDriver {
public:
    /**
     * SubID of the Zero device. 
     * This device returns endless zeroes when read from and discards any data when written to.
     **/
    static constexpr uint64 DeviceZero = 0;

public:
    uint64 Read(uint64 subID, uint64 pos, void* buffer, uint64 bufferSize) override;
    uint64 Write(uint64 subID, uint64 pos, const void* buffer, uint64 bufferSize) override;
};