#pragma once

#include "types.h"

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

    struct __attribute__((packed)) SDTHeader {
        char signature[4];
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
    };

    struct __attribute__((packed)) MADT {
        SDTHeader header;
        uint32 lapicAddress;
        uint32 flags;
        char entries[];
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

}