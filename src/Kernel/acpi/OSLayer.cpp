#include "types.h"

#include "ACPI.h"
#include "memory/MemoryManager.h"
#include "scheduler/Scheduler.h"
#include "klib/stdio.h"
#include "time/Time.h"
#include "locks/StickyLock.h"
#include "locks/QueueLock.h"

extern "C" {
#include "acpica/acpi.h"
}

extern "C" {

    ACPI_STATUS AcpiOsInitialize() { return AE_OK; }
    ACPI_STATUS AcpiOsTerminate() { return AE_OK; }

    ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
        return (ACPI_PHYSICAL_ADDRESS)MemoryManager::KernelToPhysPtr(ACPI::g_RSDP);
    }

    ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER* oldTable, ACPI_TABLE_HEADER** newTable) {
        *newTable = nullptr;
        return AE_OK;
    }

    void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS phys, ACPI_SIZE size) {
        return MemoryManager::PhysToKernelPtr((void*)phys);
    }
    void AcpiOsUnmapMemory(void* block, ACPI_SIZE size) { }

    ACPI_STATUS AcpiOsGetPhysicalAddress(void* virt, ACPI_PHYSICAL_ADDRESS* phys) {
        *phys = (ACPI_PHYSICAL_ADDRESS)MemoryManager::KernelToPhysPtr(virt);
        return AE_OK;
    }

    void* AcpiOsAllocate(ACPI_SIZE size) {
        return new char[size];
    }
    void AcpiOsFree(void* block) {
        delete[] (char*)block;
    }

    BOOLEAN AcpiOsReadable(void* virt, ACPI_SIZE size) {
        return true;
    }
    BOOLEAN AcpiOsWritable(void* virt, ACPI_SIZE size) {
        return true;
    }

    ACPI_THREAD_ID AcpiOsGetThreadID() {
        return Scheduler::ThreadGetTID();
    }

    ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE type, ACPI_OSD_EXEC_CALLBACK func, void* arg) {
        uint64 tid = Scheduler::CreateKernelThread((uint64)func, (uint64)arg);
        klog_info("ACPICA", "Created ACPICA KernelThread (tid=%i)", tid);
        return AE_OK;
    }

    void AcpiOsSleep(UINT64 millis) {
        Scheduler::ThreadWait(millis);
    }
    void AcpiOsStall(UINT32 micros) {
        uint64 start = Time::GetTSC();

        uint64 ticksPerMill = Time::GetTSCTicksPerMilli();

        uint64 ticks = ticksPerMill / 1000 * micros;
        
        while(true) {
            uint64 now = Time::GetTSC();
            if(now - start > ticks)
                break;
        }
    }

    /*ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX* outHandle) {
        *outHandle = new StickyLock();
    }
    void AcpiOsDeleteMutex(ACPI_MUTEX handle) {
        delete (StickyLock*)handle;
    }
    ACPI_STATUS AcpiOsAcquireMutex(ACPI_MUTEX handle, UINT16 to) {
        auto lock = (StickyLock*)handle;

        if(to == 0) {
            if(lock->TryLock())
                return AE_OK;
        } else if(to == 0xFFFF) {
            lock->Spinlock_Raw();
            return AE_OK;
        } else {
            uint64 start = Time::GetTSC();
            while(true) {
                uint64 now = Time::GetTSC();
                if(now - start > Time::GetTSCTicksPerMilli() * to)
                    return AE_TIME;

                if(lock->TryLock())
                    return AE_OK;
            }
        }

        return AE_TIME;
    }
    void AcpiOsReleaseMutex(ACPI_MUTEX handle) {
        ((StickyLock*)handle)->Unlock_Raw();
    }*/

    ACPI_STATUS AcpiOsCreateSemaphore(UINT32 max, UINT32 init, ACPI_SEMAPHORE* outHandle) {
        *outHandle = new QueueLock(init);
        return AE_OK;
    }
    ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE handle) {
        delete (QueueLock*)handle;
    }
    ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE handle, UINT32 units, UINT16 to) {
        auto q = (QueueLock*)handle;
        for(int i = 0; i < units; i++)
            q->Lock();
    }
    ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE handle, UINT32 units) {
        auto q = (QueueLock*)handle;
        for(int i = 0; i < units; i++)
            q->Unlock();
    }

    ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK* outHandle) {
        *outHandle = new StickyLock();
        return AE_OK;
    }
    void AcpiOsDeleteLock(ACPI_SPINLOCK handle) {
        delete (StickyLock*)handle;
    }
    ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK handle) {
        auto lock = (StickyLock*)handle;
        lock->Spinlock_Cli();
        return 0;
    }
    void AcpiOsReleaseLock(ACPI_SPINLOCK handle, ACPI_CPU_FLAGS flags)  {
        auto lock = (StickyLock*)handle;
        lock->Unlock_Cli();
    }

    ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 intLevel, ACPI_OSD_HANDLER handler, void* Context) {
        // TODO: Implement
        return AE_NOT_IMPLEMENTED;
    }
    ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 intNumber, ACPI_OSD_HANDLER handler) {
        // TODO: implement
        return AE_NOT_IMPLEMENTED;
    }

}