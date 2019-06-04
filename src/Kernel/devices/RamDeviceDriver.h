#pragma once

#include "DeviceDriver.h"
#include "Mutex.h"
#include "stl/ArrayList.h"

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

    void ReadBlock(uint64 subID, uint64 startBlock, uint64 numBlocks, void* buffer) override;
    void WriteBlock(uint64 subID, uint64 startBlock, uint64 numBlocks, const void* buffer) override;

private:
    struct DevInfo {
        char* buffer;
        uint64 blockSize;
        uint64 numBlocks;
    };

    ArrayList<DevInfo> m_Devices;
};