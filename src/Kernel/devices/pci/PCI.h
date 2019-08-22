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

    void SetPinEntry(uint8 dev, uint8 pin, uint8 gsi);

    void SetInterruptHandler(Device* dev, IDT::ISR handler);

    const Device* FindDevice(uint8 classCode, uint8 subclassCode);

    uint8 ReadConfigByte(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg);
    uint16 ReadConfigWord(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg);
    uint32 ReadConfigDWord(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg);
    uint64 ReadConfigQWord(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg);

    void WriteConfigByte(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg, uint8 val);
    void WriteConfigWord(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg, uint16 val);
    void WriteConfigDWord(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg, uint32 val);
    void WriteConfigQWord(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg, uint64 val);

}