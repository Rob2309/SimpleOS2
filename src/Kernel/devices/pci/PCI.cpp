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

    static uint8 g_VectCounter = ISRNumbers::PCIBase;

    static uint8 g_PinEntries[255 * 4] = { 0 };

    static uint64 GetMemBase(const Group& group, uint32 bus, uint32 device, uint32 func) {
        return group.baseAddr + ((uint64)bus << 20) + ((uint64)device << 15) + ((uint64)func << 12);
    }

    static uint32 ReadConfigDWord(const Group& group, uint32 bus, uint32 slot, uint32 func, uint32 offs) {
        uint64 addr = GetMemBase(group, bus, slot, func) + offs;
        return *(volatile uint32*)addr;
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

        klog_info("PCIe", "Found device: class=%02X:%02X:%02X, loc=%i:%i:%i:%i", dev.classCode, dev.subclassCode, dev.progIf, dev.group, dev.bus, dev.device, dev.function);
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
    }

    static void CheckGroup(const Group& group) {
        for(uint64 i = 0; i < 256; i++)
            for(uint64 j = 0; j < 32; j++)
                CheckDevice(group, i, j);
    }

    void Init() {
        auto mcfg = ACPI::GetMCFG();

        uint64 length = mcfg->GetEntryCount();
        for(uint64 g = 0; g < length; g++) {
            Group group;
            group.baseAddr = (uint64)MemoryManager::PhysToKernelPtr((void*)mcfg->entries[g].base);
            group.busStart = mcfg->entries[g].busStart;
            group.busEnd = mcfg->entries[g].busEnd;
            group.id = mcfg->entries[g].groupID;

            g_Groups.push_back(group);

            CheckGroup(group);
        }
    }

    const Device* FindDevice(uint8 classCode, uint8 subclassCode) {
        for(const Device& dev : g_Devices) {
            if(dev.classCode == classCode && dev.subclassCode == subclassCode)
                return &dev;
        }
        return nullptr;
    }

    void SetPinEntry(uint8 dev, uint8 pin, uint8 gsi) {
        g_PinEntries[dev * 4 + pin] = gsi;
    }

    static void SetMSI(Device* dev, uint8 apicID, IDT::ISR handler) {
        uint8 vect = g_VectCounter;
        g_VectCounter++;

        auto msi = (CapMSI*)dev->msi;
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

        klog_info("PCIe", "Allocated interrupt %i for device %i:%i:%i:%i", vect, dev->group, dev->bus, dev->device, dev->function);
    }

    void SetInterruptHandler(Device* dev, IDT::ISR handler) {
        if(dev->msi != nullptr) {
            SetMSI(dev, APIC::GetID(), handler);
        } else {
            uint8 pin = ReadConfigByte(dev->group, dev->bus, dev->device, dev->function, 0x3D);
            uint8 gsi = g_PinEntries[dev->device * 4 + pin];

            if(gsi == 0) {
                klog_error("PCI", "Could not find GSI associated with device %04X:%02X:%02X:%02X", dev->group, dev->bus, dev->device, dev->function);
                return;
            }

            IOAPIC::RegisterGSI(gsi, handler);
        }
    }

    uint8 ReadConfigByte(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg) {
        for(const auto& g : g_Groups) {
            if(g.id == groupID) {
                uint8* addr = (uint8*)GetMemBase(g, bus, device, function) + reg;
                return *addr;
            }
        }

        return 0;
    }
    uint16 ReadConfigWord(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg) {
        for(const auto& g : g_Groups) {
            if(g.id == groupID) {
                uint16* addr = (uint16*)GetMemBase(g, bus, device, function) + reg;
                return *addr;
            }
        }

        return 0;
    }
    uint32 ReadConfigDWord(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg) {
        for(const auto& g : g_Groups) {
            if(g.id == groupID) {
                uint32* addr = (uint32*)GetMemBase(g, bus, device, function) + reg;
                return *addr;
            }
        }

        return 0;
    }
    uint64 ReadConfigQWord(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg) {
        for(const auto& g : g_Groups) {
            if(g.id == groupID) {
                uint64* addr = (uint64*)GetMemBase(g, bus, device, function) + reg;
                return *addr;
            }
        }

        return 0;
    }

    void WriteConfigByte(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg, uint8 val) {
        for(const auto& g : g_Groups) {
            if(g.id == groupID) {
                uint8* addr = (uint8*)GetMemBase(g, bus, device, function) + reg;
                *addr = val;
                break;
            }
        }
    }
    void WriteConfigWord(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg, uint16 val) {
        for(const auto& g : g_Groups) {
            if(g.id == groupID) {
                uint16* addr = (uint16*)GetMemBase(g, bus, device, function) + reg;
                *addr = val;
                break;
            }
        }
    }
    void WriteConfigDWord(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg, uint32 val) {
        for(const auto& g : g_Groups) {
            if(g.id == groupID) {
                uint32* addr = (uint32*)GetMemBase(g, bus, device, function) + reg;
                *addr = val;
                break;
            }
        }
    }
    void WriteConfigQWord(uint16 groupID, uint8 bus, uint8 device, uint8 function, uint32 reg, uint64 val) {
        for(const auto& g : g_Groups) {
            if(g.id == groupID) {
                uint64* addr = (uint64*)GetMemBase(g, bus, device, function) + reg;
                *addr = val;
                break;
            }
        }
    }

};