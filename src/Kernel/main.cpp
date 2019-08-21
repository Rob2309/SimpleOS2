#include "KernelHeader.h"

#include "terminal/terminal.h"
#include "klib/stdio.h"
#include "arch/GDT.h"
#include "interrupts/IDT.h"
#include "arch/APIC.h"
#include "syscalls/SyscallHandler.h"
#include "memory/MemoryManager.h"
#include "multicore/SMP.h"

#include "fs/VFS.h"
#include "fs/TempFS.h"
#include "devices/RamDeviceDriver.h"
#include "fs/ext2/ext2.h"
#include "devices/DevFS.h"

#include "scheduler/Scheduler.h"
#include "user/User.h"
#include "klib/string.h"

#include "exec/ExecHandler.h"

#include "Config.h"

#include "arch/SSE.h"

#include "errno.h"

#include "devices/pci/PCIInit.h"

#include "syscalls/SyscallDefine.h"

#include "time/Time.h"

#include "init/Init.h"
#include "init/InitInvoke.h"

#include "acpi/ACPI.h"

extern "C" {
    #include "acpica/acpi.h"
}

static User g_RootUser;

static uint64 SetupInitProcess() {
    uint64 file;
    int64 error = VFS::Open(&g_RootUser, config_Init_Command, VFS::OpenMode_Read, file);
    if(error != OK) {
        klog_fatal("Init", "Failed to open %s (%s), aborting boot", config_Init_Command, ErrorToString(error));
        return 0;
    }
    
    VFS::NodeStats stats;
    error = VFS::Stat(file, &stats);
    if(error != OK) {
        klog_fatal("Init", "Failed to stat %s (%s)", config_Init_Command, ErrorToString(error));
        return 0;
    }
    
    uint8* buffer = new uint8[stats.size];
    error = VFS::Read(file, buffer, stats.size);
    if(error < 0) {
        klog_fatal("Init", "Failed to read %s (%s)", config_Init_Command, ErrorToString(error));
        delete[] buffer;
        return 0;
    }
    VFS::Close(file);

    uint64 pml4Entry = MemoryManager::CreateProcessMap();
    IDT::Registers regs;
    if(!ExecHandlerRegistry::Prepare(buffer, stats.size, pml4Entry, &regs)) {
        klog_error("Init", "Failed to execute init process, aborting boot");
        MemoryManager::FreeProcessMap(pml4Entry);
        delete[] buffer;
        return 0;
    }

    delete[] buffer;

    uint64 tid = Scheduler::CreateUserThread(pml4Entry, &regs, &g_RootUser);
    klog_info("Init", "Created init process with TID %i", tid);
    return tid;
}

static KernelHeader* g_KernelHeader;
Terminal::TerminalInfo g_TerminalInfo;

static UINT32 PowerButtonHandler(void* arg) {
    klog_warning("ACPI", "Power button pressed, shutting down in 5 secs");

    Scheduler::ThreadWait(5000);

    AcpiEnterSleepStatePrep(5);
    IDT::DisableInterrupts();
    AcpiEnterSleepState(5);

    return AE_OK;
}

static void ACPIThread() {
    ACPI_STATUS err;
    err = AcpiInitializeSubsystem();
    if(err != AE_OK) {
        klog_fatal("ACPI", "Failed to init ACPI: %i", err);
        while(true);
    }
    err = AcpiInitializeTables(nullptr, 16, false);
    if(err != AE_OK) {
        klog_fatal("ACPI", "Failed to init ACPI tables: %i", err);
        while(true);
    }
    err = AcpiLoadTables();
    if(err != AE_OK) {
        klog_fatal("ACPI", "Failed to load ACPI tables: %i", err);
        while(true);
    }
    err = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if(err != AE_OK) {
        klog_fatal("ACPI", "Failed to enter ACPI mode: %i", err);
        while(true);
    }
    err = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if(err != AE_OK) {
        klog_fatal("ACPI", "Failed to init ACPI Objects: %i", err);
        while(true);
    }

    err = AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON, PowerButtonHandler, nullptr);
    if(err != AE_OK) {
        klog_fatal("ACPI", "Failed to install power button handler: %i", err);
        while(true);
    }
    err = AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON, 0);
    if(err != AE_OK) {
        klog_fatal("ACPI", "Failed to enable power button handler: %i", err);
        while(true);
    }

    klog_info("ACPI", "Entered ACPI mode");

    Scheduler::ThreadExit(0);
}

static void InitThread() {
    PCI::Init(g_KernelHeader);

    klog_info("Boot", "Init KernelThread starting");

    g_RootUser.gid = 0;
    g_RootUser.uid = 0;
    kstrcpy(g_RootUser.name, "root");

    VFS::FileSystem* rootFS = new TempFS();
    VFS::Init(rootFS);

    VFS::FileSystem* devFS = new DevFS();
    int64 error = VFS::CreateFolder(&g_RootUser, "/dev", { 1, 1, 1 });
    if(error < 0) {
        klog_fatal("Init", "Failed to create /dev folder (%s)", ErrorToString(error));
        Scheduler::ThreadExit(1);
    }
    error = VFS::Mount(&g_RootUser, "/dev", devFS);
    if(error < 0) {
        klog_fatal("Init", "Failed to mount DevFS to /dev (%s)", ErrorToString(error));
        Scheduler::ThreadExit(1);
    }

    CallInitFuncs(INIT_STAGE_DEVDRIVERS);
    CallInitFuncs(INIT_STAGE_FSDRIVERS);
    CallInitFuncs(INIT_STAGE_EXECHANDLERS);

    if(g_KernelHeader->ramdiskImage.numPages != 0) {
        RamDeviceDriver* driver = (RamDeviceDriver*)DeviceDriverRegistry::GetDriver("ram");
        driver->AddDevice((char*)g_KernelHeader->ramdiskImage.buffer, 512, g_KernelHeader->ramdiskImage.numPages * 8);
    }

    kprintf("%C%s\n", 40, 200, 40, config_HelloMessage);

    klog_info("Init", "Mounting boot filesystem");

    error = VFS::CreateFolder(&g_RootUser, "/boot", { 3, 1, 1 });
    if(error < 0) {
        klog_fatal("Init", "Failed to create /boot folder (%s)", ErrorToString(error));
        Scheduler::ThreadExit(1);
    }
    error = VFS::Mount(&g_RootUser, "/boot", config_BootFS_FSID, config_BootFS_DevFile);
    if(error < 0) {
        klog_fatal("Init", "Failed to mount boot filesystem: %s", ErrorToString(error));
        Scheduler::ThreadExit(1);
    }

    uint64 tid = SetupInitProcess();

    Time::DateTime dt;
    Time::GetRTC(&dt);
    klog_info("Time", "UTC Time is %02i.%02i.20%02i %02i:%02i:%02i", dt.dayOfMonth, dt.month, dt.year, dt.hours, dt.minutes, dt.seconds);

    Scheduler::CreateKernelThread((uint64)&ACPIThread);

    Scheduler::ThreadExit(0);
}

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    g_KernelHeader = info;

    Terminal::InitTerminalInfo(&g_TerminalInfo, info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth, info->screenColorsInverted);
    Terminal::Clear(&g_TerminalInfo);

    kprintf("%CStarting SimpleOS2 Kernel\n", 40, 200, 40);
    klog_info("Boot", "Kernel at 0x%016X", info->kernelImage.buffer);

    if(!Time::Init()) {
        klog_fatal("Boot", "Boot failed...");
        while(true);
    }
    MemoryManager::Init(info);
    ACPI::g_RSDP = (ACPI::RSDPDescriptor*)info->rsdp;
    APIC::Init();
    SMP::GatherInfo(info);
    MemoryManager::InitCore(SMP::GetLogicalCoreID());
    GDT::Init(SMP::GetCoreCount());
    GDT::InitCore(SMP::GetLogicalCoreID());
    IDT::Init();
    IDT::InitCore(SMP::GetLogicalCoreID());
    APIC::InitBootCore();
    SyscallHandler::Init();
    SyscallHandler::InitCore();
    if(!SSE::InitBootCore()) {
        klog_fatal("Boot", "Boot failed...");
        while(true) ;
    }
    Scheduler::Init(SMP::GetCoreCount());

    SMP::StartCores();
    MemoryManager::EarlyFreePages(MemoryManager::KernelToPhysPtr(info->smpTrampolineBuffer), info->smpTrampolineBufferPages);

    Scheduler::CreateInitThread(InitThread);

    SMP::StartSchedulers();
    Scheduler::Start();

    klog_fatal("Boot", "Something went really wrong (Scheduler did not start), halting...");
    while(true)
        __asm__ __volatile__ ("hlt");
}