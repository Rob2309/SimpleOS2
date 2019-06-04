#pragma once

#include "DeviceDriver.h"
#include "Mutex.h"
#include "stl/ArrayList.h"

/**
 * This device driver is used for devices that reside directly in RAM (e.g. the initial ramdisk)
 **/
class RamDeviceDriver : public CharDeviceDriver {
public:
    /**
     * Register a Ramdisk to the driver.
     * @returns the SubID with which the given ramdisk can be accessed.
     **/
    uint64 AddDevice(char* buffer, uint64 size);

    uint64 Read(uint64 subID, uint64 pos, void* buffer, uint64 bufferSize) override;
    uint64 Write(uint64 subID, uint64 pos, const void* buffer, uint64 bufferSize) override;

private:
    struct DevInfo {
        char* buffer;
        uint64 size;
    };

    ArrayList<DevInfo> m_Devices;
};