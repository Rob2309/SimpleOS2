#include "types.h"

extern "C" {
#include "acpica/acpi.h"
}

#include "ACPI.h"
#include "memory/MemoryManager.h"
#include "scheduler/Scheduler.h"
#include "klib/stdio.h"
#include "time/Time.h"
#include "atomic/Atomics.h"
#include "locks/QueueLock.h"
#include "locks/StickyLock.h"
#include "arch/port.h"

extern "C" {

    ACPI_STATUS AcpiOsInitialize() { return AE_OK; }
    ACPI_STATUS AcpiOsTerminate() { return AE_OK; }

    ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
        return (ACPI_PHYSICAL_ADDRESS)MemoryManager::KernelToPhysPtr(ACPI::g_RSDP);
    }

    ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES* predefObj, ACPI_STRING* newVal) {
        *newVal = nullptr;
        return AE_OK;
    }

    ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER* oldTable, ACPI_TABLE_HEADER** newTable) {
        *newTable = nullptr;
        return AE_OK;
    }

    ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER* oldTable, ACPI_PHYSICAL_ADDRESS* newAddr, UINT32* newTableLength) {
        *newAddr = 0;
        *newTableLength = 0;
        return AE_OK;
    }

    void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS phys, ACPI_SIZE size) {
        return MemoryManager::PhysToKernelPtr((void*)phys);
    }
    void AcpiOsUnmapMemory(void* virt, ACPI_SIZE size) { }

    ACPI_STATUS AcpiOsGetPhysicalAddress(void* virt, ACPI_PHYSICAL_ADDRESS* phys) {
        if(virt == nullptr || phys == nullptr)
            return AE_BAD_PARAMETER;

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

    ACPI_THREAD_ID AcpiOsGetThreadId() {
        return Scheduler::ThreadGetTID();
    }

    static Atomic<uint64> g_NumACPICAThreads;

    static void AcpiThread(void (*func)(void*), void* arg) {
        func(arg);
        g_NumACPICAThreads.Dec();
        Scheduler::ThreadExit(0);
    }

    ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE type, ACPI_OSD_EXEC_CALLBACK func, void* arg) {
        g_NumACPICAThreads.Inc();
        uint64 tid = Scheduler::CreateKernelThread((uint64)func, (uint64)arg);
        klog_info("ACPICA", "Created kernel thread with tid %i", tid);
        return AE_OK;
    }

    void AcpiOsSleep(UINT64 millis) {
        Scheduler::ThreadWait(millis);
    }
    void AcpiOsStall(UINT32 micros) {
        Scheduler::ThreadSetSticky();

        uint32 ticks = Time::GetTSCTicksPerMilli() / 1000 * micros;

        uint64 start = Time::GetTSC();
        while(true) {
            uint64 now = Time::GetTSC();
            if(now - start >= ticks)
                break;
        }

        Scheduler::ThreadUnsetSticky();
    }

    void AcpiOsWaitEventsComplete() {
        while(g_NumACPICAThreads.Read() > 0)
            Scheduler::ThreadYield();
    }

    ACPI_STATUS AcpiOsCreateSemaphore(UINT32 max, UINT32 init, ACPI_SEMAPHORE* outHandle) {
        if(outHandle == nullptr)
            return AE_BAD_PARAMETER;
        
        *outHandle = new QueueLock(init);
        return AE_OK;
    }
    ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE handle) {
        if(handle == nullptr)
            return AE_BAD_PARAMETER;

        delete (QueueLock*)handle;
        return AE_OK;
    }
    ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE handle, UINT32 units, UINT16 to) {
        if(handle == nullptr)
            return AE_BAD_PARAMETER;

        auto lock = (QueueLock*)handle;
        for(int i = 0; i < units; i++)
            lock->Lock();
        return AE_OK;
    }
    ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE handle, UINT32 units) {
        if(handle == nullptr)
            return AE_BAD_PARAMETER;
        auto lock = (QueueLock*)handle;
        for(int i = 0; i < units; i++)
            lock->Unlock();
        return AE_OK;
    }

    ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK* outHandle) {
        if(outHandle == nullptr)
            return AE_BAD_PARAMETER;

        *outHandle = new StickyLock();
        return AE_OK;
    }
    void AcpiOsDeleteLock(ACPI_SPINLOCK handle) {
        delete (StickyLock*)handle;
    }
    ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK handle) {
        ((StickyLock*)handle)->Spinlock_Cli();
        return 0;
    }
    void AcpiOsReleaseLock(ACPI_SPINLOCK handle, ACPI_CPU_FLAGS flags) {
        ((StickyLock*)handle)->Unlock_Cli();
    }

    static ACPI_OSD_HANDLER g_AcpiISRFunc;
    static void* g_AcpiISRArg;

    static void AcpiISR(IDT::Registers* regs) {
        if(g_AcpiISRFunc == nullptr)
            return;
        g_AcpiISRFunc(g_AcpiISRArg);
    }

    ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 no, ACPI_OSD_HANDLER handler, void* arg) {
        if(no > 0xFF || handler == nullptr)
            return AE_BAD_PARAMETER;
        g_AcpiISRFunc = handler;
        g_AcpiISRArg = arg;
        IDT::SetISR(no, AcpiISR);
        return AE_OK;
    }

    ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 no, ACPI_OSD_HANDLER handler) {
        if(no > 0xFF || handler == nullptr || handler != g_AcpiISRFunc)
            return AE_BAD_PARAMETER;
        IDT::SetISR(no, nullptr);
    }

    ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS addr, UINT64* val, UINT32 width) {
        void* virt = MemoryManager::PhysToKernelPtr((void*)addr);

        switch(width) {
        case 8: *val = *(uint8*)virt; break;
        case 16: *val = *(uint16*)virt; break;
        case 32: *val = *(uint32*)virt; break;
        case 64: *val = *(uint64*)virt; break;
        }

        return AE_OK;
    }
    ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS addr, UINT64 val, UINT32 width) {
        void* virt = MemoryManager::PhysToKernelPtr((void*)addr);

        switch(width) {
        case 8: *(uint8*)virt = val; break;
        case 16: *(uint16*)virt = val; break;
        case 32: *(uint32*)virt = val; break;
        case 64: *(uint64*)virt = val; break;
        }

        return AE_OK;
    }

    ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS addr, UINT32* val, UINT32 width) {
        switch(width) {
        case 8: *val = Port::InByte(addr); break;
        case 16: *val = Port::InWord(addr); break;
        case 32: *val = Port::InDWord(addr); break;
        }

        return AE_OK;
    }
    ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS addr, UINT32 val, UINT32 width) {
        switch(width) {
        case 8: Port::OutByte(addr, val); break;
        case 16: Port::OutWord(addr, val); break;
        case 32: Port::OutDWord(addr, val); break;
        }

        return AE_OK;
    }

    ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID* pciID, UINT32 reg, UINT64* val, UINT32 width) {
        // TODO: Implement AcpiOsReadPciConfiguration
    }

    ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID* pciID, UINT32 reg, UINT64 val, UINT32 width) {
        // TODO: Implement AcpiOsWritePciConfiguration
    }

    void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf(const char* fmt, ...) {
        va_list list;
        va_start(list, fmt);

        AcpiOsVprintf(fmt, list);

        va_end(list);
    }

    void AcpiOsVprintf(const char* fmt, va_list args) {
        kvprintf(fmt, args);
    }

    void AcpiOsRedirectOutput(void* dest) { }

    UINT64 AcpiOsGetTimer() {
        uint64 tsc = Time::GetTSC();
        uint64 ticksPerMicro = Time::GetTSCTicksPerMilli() / 1000;
        uint64 ticksPer100Nano = ticksPerMicro / 10;
        
        return tsc / ticksPer100Nano;
    }

    ACPI_STATUS AcpiOsSignal(UINT32 func, void* info) {

    }



}