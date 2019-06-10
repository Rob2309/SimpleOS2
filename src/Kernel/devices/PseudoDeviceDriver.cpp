#include "PseudoDeviceDriver.h"

#include "klib/memory.h"

uint64 PseudoDeviceDriver::Read(uint64 subID, void* buffer, uint64 bufferSize) {
    if(subID == DeviceZero) {
        kmemset(buffer, 0, bufferSize);
        return bufferSize;
    } else {
        return 0;
    }
}

uint64 PseudoDeviceDriver::Write(uint64 subID, const void* buffer, uint64 bufferSize) {
    if(subID == DeviceZero) {
        return bufferSize;
    } else {
        return 0;
    }
}