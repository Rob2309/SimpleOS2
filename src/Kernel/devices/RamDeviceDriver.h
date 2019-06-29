#pragma once

#include "DeviceDriver.h"
#include "ktl/vector.h"

/**
 * This device driver is used for devices that reside directly in RAM (e.g. the initial ramdisk)
 **/
class RamDeviceDriver : public BlockDeviceDriver {
public:
    /**
     * Register a Ramdisk to the driver.
     * @returns the SubID with which the given ramdisk can be accessed.
     **/
    uint64 AddDevice(char* buffer, uint64 blockSize, uint64 numBlocks);

    uint64 GetBlockSize(uint64 subID) const override;

protected:
    void ScheduleOperation(uint64 subID, uint64 startBlock, uint64 numBlocks, bool write, void* buffer, Atomic<uint64>* finishFlag) override;

private:
    struct DevInfo {
        char* buffer;
        uint64 blockSize;
        uint64 numBlocks;
    };

    ktl::vector<DevInfo> m_Devices;
};