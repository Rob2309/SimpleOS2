#pragma once

#include "Device.h"

/**
 * This device class is used for devices that reside directly in RAM (e.g. the initial ramdisk)
 **/
class RamDevice : public Device {
public:
    RamDevice(const char* name, void* buffer, uint64 size);

    uint64 Read(uint64 pos, void* buffer, uint64 bufferSize) override;
    uint64 Write(uint64 pos, void* buffer, uint64 bufferSize) override;

private:
    char* m_Buffer;
    uint64 m_Size;
};