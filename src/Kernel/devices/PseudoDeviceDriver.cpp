#include "PseudoDeviceDriver.h"

#include "memory/memutil.h"

uint64 PseudoDeviceDriver::Read(uint64 subID, uint64 pos, void* buffer, uint64 bufferSize) {
    if(subID == DeviceZero) {
        memset(buffer, 0, bufferSize);
        return bufferSize;
    } else {
        return 0;
    }
}

uint64 PseudoDeviceDriver::Write(uint64 subID, uint64 pos, const void* buffer, uint64 bufferSize) {
    if(subID == DeviceZero) {
        return bufferSize;
    } else {
        return 0;
    }
}