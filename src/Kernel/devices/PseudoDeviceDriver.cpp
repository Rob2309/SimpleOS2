#include "PseudoDeviceDriver.h"

#include "klib/memory.h"
#include "fs/VFS.h"
#include "devices/DevFS.h"

#include "init/Init.h"

static void Init() {
    PseudoDeviceDriver* driver = new PseudoDeviceDriver();
}
REGISTER_INIT_FUNC(Init, INIT_STAGE_DEVDRIVERS);

PseudoDeviceDriver::PseudoDeviceDriver()
    : CharDeviceDriver("pseudo")
{
    DevFS::RegisterCharDevice("zero", GetDriverID(), DeviceZero);
}

int64 PseudoDeviceDriver::DeviceCommand(uint64 subID, int64 command, void* arg) {
    return OK;
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