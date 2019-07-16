#include "PseudoDeviceDriver.h"

#include "klib/memory.h"
#include "fs/VFS.h"

void PseudoDeviceDriver::Init() {
    PseudoDeviceDriver* driver = new PseudoDeviceDriver();
}

uint64 PseudoDeviceDriver::Read(uint64 subID, void* buffer, uint64 bufferSize) {
    if(subID == DeviceZero) {
        if(!kmemset_usersafe(buffer, 0, bufferSize))
            return ErrorInvalidBuffer;
        return bufferSize;
    } else {
        return ErrorInvalidDevice;
    }
}

uint64 PseudoDeviceDriver::Write(uint64 subID, const void* buffer, uint64 bufferSize) {
    if(subID == DeviceZero) {
        return bufferSize;
    } else {
        return ErrorInvalidDevice;
    }
}