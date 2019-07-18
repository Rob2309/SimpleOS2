#pragma once

#include "types.h"

namespace PCI {

    struct Device {
        uint8 bus;
        uint8 device;
        uint8 function;

        uint16 vendorID;
        uint16 deviceID;

        uint8 classCode;
        uint8 subclassCode;
        uint8 progIf;

        int numBARs;
        uint64 BARs[6];
    };

    void Init();

    Device* FindDevice(uint8 classCode, uint8 subclassCode);

}