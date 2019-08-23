#pragma once

#include "types.h"
#include "KernelHeader.h"

namespace ACPI {

    struct __attribute__((packed)) RSDPDescriptor {
        char signature[8];
        uint8 checksum;
        char oemID[6];
        uint8 revision;
        uint32 rsdtAddress;

        uint32 length;
        uint64 xsdtAddress;
        uint8 xChecksum;
        uint8 reserved[3];
    };

    #define SIGNATURE(a, b, c, d) (((uint32)(d) << 24) | ((uint32)(c) << 16) | ((uint32)(b) << 8) | ((uint32)(a)))

    struct __attribute__((packed)) SDTHeader {
        uint32 signature;
        uint32 length;
        uint8 revision;
        uint8 checksum;
        char oemID[6];
        char oemTableID[8];
        uint32 oemRevision;
        uint32 creatorID;
        uint32 creatorRevision;
    };

    struct __attribute__((packed)) XSDT {
        SDTHeader header;
        void* tables[];

        SDTHeader* FindTable(uint32 signature) const;
    };

    struct MADTEntryHeader;

    struct __attribute__((packed)) MADT {
        SDTHeader header;
        uint32 lapicAddress;
        uint32 flags;
        char entries[];

        MADTEntryHeader* GetNextEntry(uint64& pos) const;
    };

    struct __attribute__((packed)) MADTEntryHeader {
        enum Type : uint8 {
            TYPE_LAPIC = 0,
            TYPE_IOAPIC = 1,
            TYPE_ISO = 2,
            TYPE_NMI = 4,
            TYPE_LAPIC_OVERRIDE = 5,
        } entryType;
        uint8 length;
    };

    struct __attribute__((packed)) MADTEntryLAPIC {
        MADTEntryHeader header;
        uint8 processorID;
        uint8 lapicID;
        uint32 processorEnabled;
    };

    struct __attribute__((packed)) MADTEntryIOAPIC {
        MADTEntryHeader header;
        uint8 ioApicID;
        uint8 reserved;
        uint32 ioApicAddr;
        uint32 gsiBase;
    };

    struct __attribute__((packed)) MADTEntryISO {
        MADTEntryHeader header;
        uint8 bus;
        uint8 irq;
        uint32 gsi;
        uint16 flags;
    };

    struct __attribute__((packed)) MCFGEntry {
        uint64 base;
        uint16 groupID;
        uint8 busStart;
        uint8 busEnd;
        uint32 reserved;
    };

    struct __attribute__((packed)) MCFG {
        SDTHeader header;
        uint64 reserved;
        MCFGEntry entries[];

        inline uint64 GetEntryCount() const {
            return (header.length - sizeof(MCFG)) / sizeof(MCFGEntry);
        }
    };

    /**
     * Has to be called before retrieving any ACPI tables
     **/
    void InitEarlyTables(KernelHeader* header);
    /**
     * Starts the ACPICA subsystem and puts the system into ACPI mode
     **/
    bool StartSystem();

    const MADT* GetMADT();
    const MCFG* GetMCFG();
    const RSDPDescriptor* GetRSDP();

    typedef void* Handle;
    Handle GetPCIRootBridge();

}