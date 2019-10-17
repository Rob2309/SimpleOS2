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
    PseudoDeviceDriver();

    int64 DeviceCommand(uint64 subID, int64 command, void* arg) override;

    uint64 Read(uint64 subID, void* buffer, uint64 bufferSize) override;
    uint64 Write(uint64 subID, const void* buffer, uint64 bufferSize) override;
};