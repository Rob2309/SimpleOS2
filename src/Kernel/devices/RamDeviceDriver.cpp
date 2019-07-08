#include "RamDeviceDriver.h"

#include "klib/memory.h"

uint64 RamDeviceDriver::AddDevice(char* buffer, uint64 blockSize, uint64 numBlocks) {
    uint64 res = m_Devices.size();
    m_Devices.push_back({ buffer, blockSize, numBlocks });
    return res;
}

uint64 RamDeviceDriver::GetBlockSize(uint64 subID) const {
    return m_Devices[subID].blockSize;
}

void RamDeviceDriver::ScheduleOperation(uint64 subID, uint64 startBlock, uint64 numBlocks, bool write, void* buffer, Atomic<uint64>* finishFlag) {
    const DevInfo& dev = m_Devices[subID];

    if(write) {
        kmemcpy_usersafe(dev.buffer + startBlock * GetBlockSize(subID), buffer, GetBlockSize(subID) * numBlocks);
    } else {
        kmemcpy_usersafe(buffer, dev.buffer + startBlock * GetBlockSize(subID), GetBlockSize(subID) * numBlocks);
    }

    finishFlag->Write(1);
}