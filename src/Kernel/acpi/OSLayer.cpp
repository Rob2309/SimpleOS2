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
#include "devices/pci/PCI.h"
#include "arch/APIC.h"
#include "arch/IOAPIC.h"

extern "C" {

    ACPI_STATUS AcpiOsInitialize() { return AE_OK; }
    ACPI_STATUS AcpiOsTerminate() { return AE_OK; }

    ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
        return (ACPI_PHYSICAL_ADDRESS)MemoryManager::KernelToPhysPtr(ACPI::GetRSDP());
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

    struct Job {
        void (*func)(void*);
        void* arg;
    };

    static Atomic<uint64> g_NumRunningJobs;
    static StickyLock g_JobLock;
    static uint64 g_JobPushIndex = 0;
    static uint64 g_JobPopIndex = 0;
    static Job g_JobQueue[128];

    void AcpiJobThread() {
        klog_info("ACPI", "Job thread starting...");

        while(true) {
            g_JobLock.Spinlock_Cli();
            if(g_JobPopIndex < g_JobPushIndex) {
                auto job = g_JobQueue[g_JobPopIndex % 128];
                g_JobPopIndex++;
                g_JobLock.Unlock_Cli();

                job.func(job.arg);
                
                g_NumRunningJobs.Dec();
            } else {
                g_JobLock.Unlock_Cli();
                Scheduler::ThreadYield();
            }
        }
    }

    ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE type, ACPI_OSD_EXEC_CALLBACK func, void* arg) {
        g_NumRunningJobs.Inc();
        
        g_JobLock.Spinlock_Raw();
        g_JobQueue[g_JobPushIndex % 128] = { func, arg };
        g_JobPushIndex++;
        g_JobLock.Unlock_Raw();

        return AE_OK;
    }

    void AcpiOsSleep(UINT64 millis) {
        Scheduler::ThreadWait(millis);
    }
    void AcpiOsStall(UINT32 micros) {
        Scheduler::ThreadSetSticky();

        uint64 ticks = Time::GetTSCTicksPerMilli() / 1000 * micros;

        uint64 start = Time::GetTSC();
        while(true) {
            uint64 now = Time::GetTSC();
            if(now - start >= ticks)
                break;
        }

        Scheduler::ThreadUnsetSticky();
    }

    void AcpiOsWaitEventsComplete() {
        while(g_NumRunningJobs.Read() > 0)
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
        
        IOAPIC::RegisterIRQ(no, AcpiISR);

        klog_info("ACPI", "Installed ISR %u", no);

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
        switch(width) {
        case 8:  *val = PCI::ReadConfigByte(pciID->Segment, pciID->Bus, pciID->Device, pciID->Function, reg); break;
        case 16: *val = PCI::ReadConfigWord(pciID->Segment, pciID->Bus, pciID->Device, pciID->Function, reg); break;
        case 32: *val = PCI::ReadConfigDWord(pciID->Segment, pciID->Bus, pciID->Device, pciID->Function, reg); break;
        case 64: *val = PCI::ReadConfigQWord(pciID->Segment, pciID->Bus, pciID->Device, pciID->Function, reg); break;
        }

        return AE_OK;
    }

    ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID* pciID, UINT32 reg, UINT64 val, UINT32 width) {
        switch(width) {
        case 8: PCI::WriteConfigByte(pciID->Segment, pciID->Bus, pciID->Device, pciID->Function, reg, val); break;
        case 16: PCI::WriteConfigWord(pciID->Segment, pciID->Bus, pciID->Device, pciID->Function, reg, val); break;
        case 32: PCI::WriteConfigDWord(pciID->Segment, pciID->Bus, pciID->Device, pciID->Function, reg, val); break;
        case 64: PCI::WriteConfigQWord(pciID->Segment, pciID->Bus, pciID->Device, pciID->Function, reg, val); break;
        }

        return AE_OK;
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

    ACPI_STATUS AcpiOsEnterSleep (UINT8 sleepState, UINT32 regAValue, UINT32 regBValue) {
        return AE_OK;
    }

}