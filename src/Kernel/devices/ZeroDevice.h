#pragma once

#include "Device.h"

/**
 * This device returns endless zeroes and eats everything you write to it
 **/
class ZeroDevice : public Device
{
public:
    ZeroDevice(const char* name);

    uint64 Read(uint64 pos, void* buffer, uint64 bufferSize) override;
    uint64 Write(uint64 pos, void* buffer, uint64 bufferSize) override;
};