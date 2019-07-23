#pragma once

#include "types.h"

#include "interrupts/IDT.h"

namespace PCI {

    struct Device {
        uint16 group;
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

        uint64 memBase;

        void* msi;
    };

    void SetMSI(Device* dev, uint8 apicID, IDT::ISR handler);

    const Device* FindDevice(uint8 classCode, uint8 subclassCode);

}