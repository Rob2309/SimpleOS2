#include "ACPI.h"

#include "memory/MemoryManager.h"

namespace ACPI {

    RSDPDescriptor* g_RSDP;

    SDTHeader* XSDT::FindTable(uint32 signature) {
        uint32 numEntries = (header.length - sizeof(XSDT)) / 8;
        for(int i = 0; i < numEntries; i++) {
            SDTHeader* entry = (SDTHeader*)MemoryManager::PhysToKernelPtr(tables[i]);
            if(entry->signature == signature)
                return entry;
        }
        return nullptr;
    }

    MADTEntryHeader* MADT::GetNextEntry(uint64& pos) {
        if(pos >= header.length - sizeof(MADT))
            return nullptr;

        MADTEntryHeader* entry = (MADTEntryHeader*)&entries[pos];
        pos += entry->length;
        return entry;
    }

}