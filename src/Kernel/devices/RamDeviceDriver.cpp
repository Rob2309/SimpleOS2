#include "RamDeviceDriver.h"

#include "memory/memutil.h"

uint64 RamDeviceDriver::AddDevice(char* buffer, uint64 blockSize, uint64 numBlocks) {
    uint64 res = m_Devices.size();
    m_Devices.push_back({ buffer, blockSize, numBlocks });
    return res;
}

uint64 RamDeviceDriver::GetBlockSize(uint64 subID) const {
    return m_Devices[subID].blockSize;
}

void RamDeviceDriver::ReadBlock(uint64 subID, uint64 startBlock, uint64 numBlocks, void* buffer) {
    const DevInfo& dev = m_Devices[subID];

    memcpy(buffer, &dev.buffer[startBlock * dev.blockSize], numBlocks * dev.blockSize);
}
void RamDeviceDriver::WriteBlock(uint64 subID, uint64 startBlock, uint64 numBlocks, const void* buffer) {
    const DevInfo& dev = m_Devices[subID];

    memcpy(&dev.buffer[startBlock * dev.blockSize], buffer, numBlocks * dev.blockSize);
}
