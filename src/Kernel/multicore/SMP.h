#pragma once

#include "types.h"
#include "KernelHeader.h"

namespace SMP {

    constexpr uint64 MaxCoreCount = 128;

    /**
     * Search the ACPI structures for information about the available CPU cores
     **/
    void GatherInfo(KernelHeader* header);
    /**
     * Starts up the CPU cores into a waiting state
     **/
    void StartCores();
    /**
     * Starts the scheduler on every CPU core except the boot core.
     **/
    void StartSchedulers();

    /**
     * Returns the number of logical CPU cores available in the system.
     **/
    uint64 GetCoreCount();
    /**
     * Returns the logical core ID of the calling processor core.
     * This ID is used in many different parts of the kernel.
     **/
    uint64 GetLogicalCoreID();
    /**
     * Returns the APIC ID of the given logical core.
     * Used to Send IPIs to a logical core.
     **/
    uint64 GetApicID(uint64 logicalCore);

}