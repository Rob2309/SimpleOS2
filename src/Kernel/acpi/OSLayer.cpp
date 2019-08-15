#include "types.h"

#include "ACPI.h"
#include "acpica/acpi.h"

#include "memory/MemoryManager.h"

extern "C" {

ACPI_STATUS AcpiOsInitialize() { }
ACPI_STATUS AcpiOsTerminate() { }

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
    return (ACPI_PHYSICAL_ADDRESS)MemoryManager::KernelToPhysPtr(ACPI::g_RSDP);
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue) {
    *NewValue = nullptr;
    return 0;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER* ExistingTable, ACPI_TABLE_HEADER** NewTable) {
    *NewTable = nullptr;
    return 0;
}



}