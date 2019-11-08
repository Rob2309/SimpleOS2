#pragma once

#include "types.h"

#include "interrupts/IDT.h"
#include "../DeviceDriver.h"

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

        uint16 subsystemID;
        uint16 subsystemVendorID;

        uint64 memBase;

        void* msi;
    };

    struct DriverInfo {
        uint16 vendorID;
        uint16 deviceID;

        uint8 classCode;
        uint8 subclassCode;
        uint8 progIf;

        uint16 subsystemID;
        uint16 subsystemVendorID;
    };

    typedef void (*PCIDriverFactory)(const Device& dev);
    void RegisterDriver(const DriverInfo& info, PCIDriverFactory factory);

    void SetInterruptHandler(const Device& dev, IDT::ISR handler);

    uint8 ReadConfigByte(const Device& dev, uint32 reg);
    uint16 ReadConfigWord(const Device& dev, uint32 reg);
    uint32 ReadConfigDWord(const Device& dev, uint32 reg);
    uint64 ReadConfigQWord(const Device& dev, uint32 reg);

    void WriteConfigByte(const Device& dev, uint32 reg, uint8 val);
    void WriteConfigWord(const Device& dev, uint32 reg, uint16 val);
    void WriteConfigDWord(const Device& dev, uint32 reg, uint32 val);
    void WriteConfigQWord(const Device& dev, uint32 reg, uint64 val);

}