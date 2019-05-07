#include "ZeroDevice.h"

#include "memory/memutil.h"

ZeroDevice::ZeroDevice(const char* name)
    : Device(name)
{ }

uint64 ZeroDevice::Read(uint64 pos, void* buffer, uint64 bufferSize)
{
    memset(buffer, 0, bufferSize);
    return bufferSize;
}

uint64 ZeroDevice::Write(uint64 pos, void* buffer, uint64 bufferSize)
{ 
    return bufferSize;
}