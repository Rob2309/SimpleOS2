#include "PCI.h"
#include "PCIInit.h"

#include "acpi/ACPI.h"
#include "memory/MemoryManager.h"

#include "klib/stdio.h"

#include "ktl/vector.h"

#include "arch/APIC.h"
#include "arch/IOAPIC.h"

namespace PCI {

    struct Group {
        uint16 id;
        uint64 baseAddr;
        uint8 busStart;
        uint8 busEnd;
    };
    
    constexpr uint8 CAP_MSI = 0x05;

    constexpr uint16 MSI_CONTROL_64BIT = 0x80;
    constexpr uint16 MSI_CONTROL_ENABLE = 0x01;

    struct __attribute__((packed)) CapHeader {
        uint8 typeID;
        uint8 nextPtr;
    };

    struct __attribute__((packed)) CapMSI {
        CapHeader header;
        uint16 control;
    };

    struct __attribute__((packed)) CapMSI32 {
        CapHeader header;
        uint16 control;
        uint32 address;
        uint16 data;
    };

    struct __attribute__((packed)) CapMSI64 {
        CapHeader header;
        uint16 control;
        uint32 addressLow;
        uint32 addressHigh;
        uint16 data;
    };

    static ktl::vector<Group> g_Groups;
    static ktl::vector<Device> g_Devices;

    struct DriverReg {
        DriverInfo info;
        PCIDriverFactory factory;
    };
    static ktl::vector<DriverReg> g_Drivers;

    static uint8 g_VectCounter = ISRNumbers::PCIBase;

    static uint64 GetMemBase(const Group& group, uint32 bus, uint32 device, uint32 func) {
        return group.baseAddr + ((uint64)bus << 20) + ((uint64)device << 15) + ((uint64)func << 12);
    }

    static uint32 ReadConfigDWord(const Group& group, uint32 bus, uint32 slot, uint32 func, uint32 offs) {
        uint64 addr = GetMemBase(group, bus, slot, func) + offs;
        return *(volatile uint32*)addr;
    }

    static PCIDriverFactory FindDriver(const Device& dev) {
        for(const auto& [info, factory] : g_Drivers) {
            if(info.vendorID != 0xFFFF && info.vendorID != dev.vendorID)
                continue;
            if(info.deviceID != 0xFFFF && info.deviceID != dev.deviceID)
                continue;
            if(info.classCode != 0xFF && info.classCode != dev.classCode)
                continue;
            if(info.subclassCode != 0xFF && info.subclassCode != dev.subclassCode)
                continue;
            if(info.progIf != 0xFF && info.progIf != dev.progIf)
                continue;
            if(info.subsystemID != 0xFFFF && info.subsystemID != dev.subsystemID)
                continue;
            if(info.subsystemVendorID != 0xFFFF && info.subsystemVendorID != dev.subsystemVendorID)
                continue;

            return factory;
        }

        return nullptr;
    }

    void RegisterDriver(const DriverInfo& info, PCIDriverFactory factory) {
        g_Drivers.push_back({ info, factory });
        klog_info("PCIe", "Registered driver for vendorID=%04X deviceID=%04X class=%02X:%02X:%02X subsystem=%04X subVendor=%04X", info.vendorID, info.deviceID, info.classCode, info.subclassCode, info.progIf, info.subsystemID, info.subsystemVendorID);
    }

    static bool CheckFunction(const Group& group, uint32 bus, uint32 device, uint32 func) {
        uint32 id = ReadConfigDWord(group, bus, device, func, 0);
        if((id & 0xFFFF) == 0xFFFF)
            return false;

        Device dev = { 0 };
        dev.vendorID = (id & 0xFFFF);
        dev.deviceID = ((id >> 16) & 0xFFFF);
        dev.group = group.id;
        dev.bus = bus;
        dev.device = device;
        dev.function = func;

        uint8 headerType = ReadConfigDWord(group, bus, device, func, 0x0E);
        headerType &= 0x7F;

        uint32 classVal = ReadConfigDWord(group, bus, device, func, 0x08);
        uint8 classCode = classVal >> 24;
        uint8 subclass = (classVal >> 16) & 0xFF;
        uint8 progif = (classVal >> 8) & 0xFF;

        uint32 subsystemVal = ReadConfigDWord(group, bus, device, func, 0x2C);
        dev.subsystemVendorID = subsystemVal & 0xFFFF;
        dev.subsystemID = (subsystemVal >> 16) & 0xFFFF;

        dev.classCode = classCode;
        dev.subclassCode = subclass;
        dev.progIf = progif;

        dev.memBase = GetMemBase(group, bus, device, func);

        dev.numBARs = 0;
        for(int i = 0; i < 6; i++) {
            uint32 bar = ReadConfigDWord(group, bus, device, func, 0x10 + i * 4);

            if(bar & 0x1) {
                dev.BARs[dev.numBARs] = bar & 0xFFFFFFFC;
                dev.numBARs++;
            } else {
                uint8 type = (bar & 0x7) >> 1;
                if(type == 0) {
                    dev.BARs[dev.numBARs] = bar & 0xFFFFFFF0;
                    dev.numBARs++;
                } else if(type == 1) {
                    uint64 bar2 = ReadConfigDWord(group, bus, device, func, 0x10 + i * 4 + 4);
                    dev.BARs[dev.numBARs] = (bar2 << 32) | (bar & 0xFFFFFFF0);
                    dev.numBARs++;
                    i++;
                }
            }
        }

        dev.msi = nullptr;
        uint8 status = ReadConfigDWord(group, bus, device, func, 0x06);
        if(status & 0x10) {
            uint8 capPtr = ReadConfigDWord(group, bus, device, func, 0x34);

            while(true) {
                CapHeader* cap = (CapHeader*)(dev.memBase + capPtr);

                if(cap->typeID == CAP_MSI) {
                    auto msi = (CapMSI*)cap;
                    if(msi->control & MSI_CONTROL_64BIT) {
                        auto msi64 = (CapMSI64*)msi;
                        msi64->addressHigh = 0;
                    } else {
                        auto msi32 = (CapMSI32*)msi;
                    }

                    dev.msi = msi;
                }

                capPtr = cap->nextPtr;
                if(capPtr == 0)
                    break;
            }
        }

        auto factory = FindDriver(dev);

        klog_info("PCIe", "Found device: class=%02X:%02X:%02X, loc=%04X:%02X:%02X:%02X, vendor=%04X, subsystem=%04X %s", dev.classCode, dev.subclassCode, dev.progIf, dev.group, dev.bus, dev.device, dev.function, dev.vendorID, dev.subsystemID, factory != nullptr ? "(Driver available)" : "");
        g_Devices.push_back(dev);

        return true;
    }

    static bool CheckDevice(const Group& group, uint32 bus, uint32 device) {
        if(!CheckFunction(group, bus, device, 0))
            return false;
        
        uint16 headerType = ReadConfigDWord(group, bus, device, 0, 0x0E);
        if(headerType & 0x80) {
            for(int i = 1; i < 8; i++)
                CheckFunction(group, bus, device, i);
        }
        return true;
    }

    static void CheckGroup(const Group& group) {
        for(uint64 i = group.busStart; i <= group.busEnd; i++)
            for(uint64 j = 0; j < 32; j++)
                CheckDevice(group, i, j);
    }

    void ProbeDevices() {
        auto mcfg = ACPI::GetMCFG();

        uint64 length = mcfg->GetEntryCount();
        for(uint64 g = 0; g < length; g++) {
            Group group;
            group.baseAddr = (uint64)MemoryManager::PhysToKernelPtr((void*)mcfg->entries[g].base);
            group.busStart = mcfg->entries[g].busStart;
            group.busEnd = mcfg->entries[g].busEnd;
            group.id = mcfg->entries[g].groupID;

            g_Groups.push_back(group);

            klog_info("PCIe", "Group: %016X %02X %02X %04X", group.baseAddr, group.busStart, group.busEnd, group.id);

            CheckGroup(group);
        }

        for(const auto& dev : g_Devices) {
            auto factory = FindDriver(dev);
            if(factory != nullptr)
                factory(dev);
        }
    }

    static void SetMSI(const Device& dev, uint8 apicID, IDT::ISR handler) {
        uint8 vect = g_VectCounter;
        g_VectCounter++;

        auto msi = (CapMSI*)dev.msi;
        if(msi->control & MSI_CONTROL_64BIT) {
            auto msi64 = (CapMSI64*)msi;
            uint16 control = msi64->control;
            control &= 0xFF8F;
            control |= 0x1;
            msi64->control = control;

            msi64->addressHigh = 0;
            msi64->addressLow = 0xFEE00000 | ((uint32)apicID << 12);
            msi64->data = vect;
        } else {
            auto msi32 = (CapMSI32*)msi;
            uint16 control = msi32->control;
            control &= 0xFF8F;
            control |= 0x1;
            msi32->control = control;

            msi32->address = 0xFEE00000 | ((uint32)apicID << 12);
            msi32->data = vect;
        }

        IDT::SetInternalISR(vect, handler);

        klog_info("PCIe", "Allocated interrupt %i for device %i:%i:%i:%i", vect, dev.group, dev.bus, dev.device, dev.function);
    }

    void SetInterruptHandler(const Device& dev, IDT::ISR handler) {
        if(dev.msi != nullptr) {
            SetMSI(dev, APIC::GetID(), handler);
        } else {
            uint8 pin = ReadConfigByte(dev, 0x3D);
            ACPI::AcpiIRQInfo irqInfo;
            if(!ACPI::GetPCIDeviceIRQ(dev.device, pin, irqInfo)) {
                klog_error("PCI", "Could not find interrupt associated with device %04X:%02X:%02X:%02X", dev.group, dev.bus, dev.device, dev.function);
            }

            if(irqInfo.isGSI)
                IOAPIC::RegisterGSI(irqInfo.number, handler);
            else
                IOAPIC::RegisterIRQ(irqInfo.number, handler);
        }
    }

    uint8 ReadConfigByte(const Device& dev, uint32 reg) {
        for(const auto& g : g_Groups) {
            if(g.id == dev.group) {
                uint8* addr = (uint8*)GetMemBase(g, dev.bus, dev.device, dev.function) + reg;
                return *addr;
            }
        }

        return 0;
    }
    uint16 ReadConfigWord(const Device& dev, uint32 reg) {
        for(const auto& g : g_Groups) {
            if(g.id == dev.group) {
                uint16* addr = (uint16*)GetMemBase(g, dev.bus, dev.device, dev.function) + reg;
                return *addr;
            }
        }

        return 0;
    }
    uint32 ReadConfigDWord(const Device& dev, uint32 reg) {
        for(const auto& g : g_Groups) {
            if(g.id == dev.group) {
                uint32* addr = (uint32*)GetMemBase(g, dev.bus, dev.device, dev.function) + reg;
                return *addr;
            }
        }

        return 0;
    }
    uint64 ReadConfigQWord(const Device& dev, uint32 reg) {
        for(const auto& g : g_Groups) {
            if(g.id == dev.group) {
                uint64* addr = (uint64*)GetMemBase(g, dev.bus, dev.device, dev.function) + reg;
                return *addr;
            }
        }

        return 0;
    }

    void WriteConfigByte(const Device& dev, uint32 reg, uint8 val) {
        for(const auto& g : g_Groups) {
            if(g.id == dev.group) {
                uint8* addr = (uint8*)GetMemBase(g, dev.bus, dev.device, dev.function) + reg;
                *addr = val;
                break;
            }
        }
    }
    void WriteConfigWord(const Device& dev, uint32 reg, uint16 val) {
        for(const auto& g : g_Groups) {
            if(g.id == dev.group) {
                uint16* addr = (uint16*)GetMemBase(g, dev.bus, dev.device, dev.function) + reg;
                *addr = val;
                break;
            }
        }
    }
    void WriteConfigDWord(const Device& dev, uint32 reg, uint32 val) {
        for(const auto& g : g_Groups) {
            if(g.id == dev.group) {
                uint32* addr = (uint32*)GetMemBase(g, dev.bus, dev.device, dev.function) + reg;
                *addr = val;
                break;
            }
        }
    }
    void WriteConfigQWord(const Device& dev, uint32 reg, uint64 val) {
        for(const auto& g : g_Groups) {
            if(g.id == dev.group) {
                uint64* addr = (uint64*)GetMemBase(g, dev.bus, dev.device, dev.function) + reg;
                *addr = val;
                break;
            }
        }
    }

};