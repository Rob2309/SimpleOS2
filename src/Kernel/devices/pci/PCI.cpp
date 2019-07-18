#include "PCI.h"

#include "types.h"
#include "arch/port.h"

#include "klib/stdio.h"

#include "ktl/vector.h"

namespace PCI {

    static ktl::vector<Device> g_Devices;

    static uint32 ReadConfigDWord(uint32 bus, uint32 slot, uint32 func, uint32 offs) {
        uint32 addr = (bus << 16) | (slot << 11) | (func << 8) | offs | (1 << 31);
        Port::OutDWord(0xCF8, addr);
        return Port::InDWord(0xCFC);
    }

    static uint16 ReadConfigWord(uint32 bus, uint32 slot, uint32 func, uint32 offs) {
        uint32 dword = ReadConfigDWord(bus, slot, func, offs & 0xFC);
        if(offs % 4 == 0)
            return dword & 0xFFFF;
        else
            return (dword >> 16) & 0xFFFF;
    }

    static bool CheckFunction(uint32 bus, uint32 device, uint32 func) {
        uint32 id = ReadConfigDWord(bus, device, func, 0);
        if((id & 0xFFFF) == 0xFFFF)
            return false;

        Device dev = { 0 };
        dev.vendorID = (id & 0xFFFF);
        dev.deviceID = ((id >> 16) & 0xFFFF);
        dev.bus = bus;
        dev.device = device;
        dev.function = func;

        uint8 headerType = ReadConfigWord(bus, device, func, 0x0E);
        headerType &= 0x7F;

        uint32 classVal = ReadConfigDWord(bus, device, func, 0x08);
        uint8 classCode = classVal >> 24;
        uint8 subclass = (classVal >> 16) & 0xFF;
        uint8 progif = (classVal >> 8) & 0xFF;

        dev.classCode = classCode;
        dev.subclassCode = subclass;
        dev.progIf = progif;

        dev.numBARs = 0;
        for(int i = 0; i < 6; i++) {
            uint32 bar = ReadConfigDWord(bus, device, func, 0x10 + i * 4);

            if(bar & 0x1) {
                dev.BARs[dev.numBARs] = bar & 0xFFFFFFFC;
                dev.numBARs++;
            } else {
                uint8 type = (bar & 0x7) >> 1;
                if(type == 0) {
                    dev.BARs[dev.numBARs] = bar & 0xFFFFFFF0;
                    dev.numBARs++;
                } else if(type == 1) {
                    uint64 bar2 = ReadConfigDWord(bus, device, func, 0x10 + i * 4 + 4);
                    dev.BARs[dev.numBARs] = (bar2 << 32) | (bar & 0xFFFFFFF0);
                    dev.numBARs++;
                    i++;
                }
            }
        }

        g_Devices.push_back(dev);

        return true;
    }

    static bool CheckDevice(uint32 bus, uint32 device) {
        if(!CheckFunction(bus, device, 0))
            return false;
        
        uint16 headerType = ReadConfigWord(bus, device, 0, 0x0E);
        if(headerType & 0x80) {
            for(int i = 1; i < 8; i++)
                CheckFunction(bus, device, i);
        }
    }

    void Init() {
        for(uint64 i = 0; i < 256; i++)
            for(uint64 j = 0; j < 32; j++)
                CheckDevice(i, j);

        for(const Device& dev : g_Devices) {
            klog_info("PCI", "Found device: class=%X:%X:%X, loc=%i:%i:%i, bar0=%X", dev.classCode, dev.subclassCode, dev.function, dev.bus, dev.device, dev.function, dev.BARs[0]);
        }
    }

};