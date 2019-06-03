#include "RamDeviceDriver.h"

#include "memory/memutil.h"

uint64 RamDeviceDriver::AddDevice(char* buffer, uint64 size) {
    uint64 res = m_Devices.size();
    m_Devices.push_back({ buffer, size });
    return res;
}

uint64 RamDeviceDriver::Read(uint64 subID, uint64 pos, void* buffer, uint64 bufferSize) {
    const DevInfo& info = m_Devices[subID];

    uint64 rem = info.size - pos;
    if(rem < bufferSize)
        bufferSize = rem;

    memcpy(buffer, info.buffer + pos, bufferSize);
    return bufferSize;
}

uint64 RamDeviceDriver::Write(uint64 subID, uint64 pos, const void* buffer, uint64 bufferSize) {
    const DevInfo& info = m_Devices[subID];

    uint64 rem = info.size - pos;
    if(rem < bufferSize)
        bufferSize = rem;

    memcpy(info.buffer + pos, buffer, bufferSize);
    return bufferSize;
}