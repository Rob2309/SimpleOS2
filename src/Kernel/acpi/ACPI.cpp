#include "ACPI.h"

#include "memory/MemoryManager.h"
#include "klib/stdio.h"
#include "interrupts/IDT.h"
#include "scheduler/Scheduler.h"
#include "devices/pci/PCI.h"

extern "C" {
    #include "acpica/acpi.h"
}

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

    static void AcpiEventHandler(UINT32 eventType, ACPI_HANDLE dev, UINT32 eventNumber, void* arg) {
        if(eventType == ACPI_EVENT_TYPE_FIXED && eventNumber == ACPI_EVENT_POWER_BUTTON) {
            klog_warning("ACPI", "Power button pressed, shutting down in 3 seconds...");

            AcpiOsStall(3000000);

            AcpiEnterSleepStatePrep(ACPI_STATE_S5);
            IDT::DisableInterrupts();
            AcpiEnterSleepState(ACPI_STATE_S5);
        }
    }

    static ACPI_BUFFER RunAcpi(const char* obj, ACPI_OBJECT* args, uint64 argCount) {
        ACPI_BUFFER retVal = { ACPI_ALLOCATE_BUFFER, nullptr };
        ACPI_OBJECT_LIST argList = { argCount, args };
        ACPI_STATUS err = AcpiEvaluateObject(nullptr, (char*)obj, &argList, &retVal);
        if(err != AE_OK) {
            klog_error("ACPI", "Failed to execute %s: %i", obj, err);
        }
        return retVal;
    }

    static void FreeBuffer(const ACPI_BUFFER& buffer) {
        if(buffer.Pointer != nullptr)
            AcpiOsFree(buffer.Pointer);
    }

    static ACPI_STATUS AcpiWalk (ACPI_HANDLE dev, UINT32 nestLevel, void* ctx, void** retVal) {
        ACPI_DEVICE_INFO* info;
        AcpiGetObjectInfo(dev, &info);

        if(info->Flags & ACPI_PCI_ROOT_BRIDGE) {
            klog_info("ACPI", "Found PCI Root bridge");

            ACPI_BUFFER buffer = { ACPI_ALLOCATE_BUFFER, nullptr };
            ACPI_STATUS err = AcpiGetIrqRoutingTable(dev, &buffer);
            if(err != AE_OK) {
                klog_error("ACPI", "Failed to retrieve Irq info");
                return AE_ERROR;
            }
            
            auto table = (ACPI_PCI_ROUTING_TABLE*)buffer.Pointer;
            while(true) {
                if(table->Length == 0)
                    break;

                uint8 devID = table->Address >> 16;
                if(*(uint32*)table->Source == 0) {
                    PCI::SetPinEntry(devID, table->Pin, table->SourceIndex);
                } else {
                    klog_warning("ACPI", "PCI device %02X uses unsupported IRQ routing mode", devID);
                }

                table = (ACPI_PCI_ROUTING_TABLE*)((char*)table + table->Length);
            }
        }

        ACPI_FREE(info);
        return AE_OK;
    }

    void AcpiSystemThread(KernelHeader* header) {
        ACPI_STATUS err;
        err = AcpiInitializeSubsystem();
        if(err != AE_OK) {
            klog_fatal("ACPI", "Failed to init ACPI: %i", err);
            Scheduler::ThreadExit(1);
        }
        err = AcpiInitializeTables(nullptr, 16, false);
        if(err != AE_OK) {
            klog_fatal("ACPI", "Failed to init ACPI tables: %i", err);
            Scheduler::ThreadExit(1);
        }
        err = AcpiLoadTables();
        if(err != AE_OK) {
            klog_fatal("ACPI", "Failed to load ACPI tables: %i", err);
            Scheduler::ThreadExit(1);
        }

        err = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
        if(err != AE_OK) {
            klog_fatal("ACPI", "Failed to enter ACPI mode: %i", err);
            Scheduler::ThreadExit(1);
        }
        err = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
        if(err != AE_OK) {
            klog_fatal("ACPI", "Failed to init ACPI Objects: %i", err);
            Scheduler::ThreadExit(1);
        }

        err = AcpiUpdateAllGpes();
        if(err != AE_OK) {
            klog_fatal("ACPI", "Failed to update GPEs: %i", err);
            Scheduler::ThreadExit(1);
        }

        err = AcpiInstallGlobalEventHandler(AcpiEventHandler, nullptr);
        if(err != AE_OK) {
            klog_fatal("ACPI", "Failed to install power button handler: %i", err);
            Scheduler::ThreadExit(1);
        }
        err = AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON, 0);
        if(err != AE_OK) {
            klog_fatal("ACPI", "Failed to enable power button handler: %i", err);
            Scheduler::ThreadExit(1);
        }

        klog_info("ACPI", "Entered ACPI mode");

        ACPI_OBJECT arg;
        arg.Type = ACPI_TYPE_INTEGER;
        arg.Integer.Value = 1;
        auto ret = RunAcpi("\\_PIC", &arg, 1);
        FreeBuffer(ret);

        void* retVal;
        AcpiGetDevices(nullptr, AcpiWalk, nullptr, &retVal);

        Scheduler::ThreadExit(0);
    }

}