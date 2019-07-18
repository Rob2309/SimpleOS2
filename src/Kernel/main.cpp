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

#include "scheduler/Scheduler.h"
#include "user/User.h"
#include "klib/string.h"

#include "exec/ExecHandler.h"

#include "Config.h"

#include "arch/SSE.h"

#include "errno.h"

#include "devices/pci/PCI.h"

static User g_RootUser;

static uint64 SetupInitProcess() {
    uint64 file;
    int64 error = VFS::Open(&g_RootUser, config_Init_Command, VFS::OpenMode_Read, file);
    if(error != OK) {
        klog_fatal("Init", "Failed to open %s (%s), aborting boot", config_Init_Command, ErrorToString(error));
        return 0;
    }
    
    VFS::NodeStats stats;
    VFS::Stat(file, &stats);
    
    uint8* buffer = new uint8[stats.size];
    VFS::Read(file, buffer, stats.size);
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

static void InitThread() {
    PCI::Init();

    klog_info("Boot", "Init KernelThread starting");

    g_RootUser.gid = 0;
    g_RootUser.uid = 0;
    kstrcpy(g_RootUser.name, "root");

    for(uint64 i = 0; i < sizeof(config_DeviceDriverInitFuncs) / sizeof(InitFunc); i++) {
        klog_info("Boot", "Starting DeviceDriver %i of %i", i+1, sizeof(config_DeviceDriverInitFuncs) / sizeof(InitFunc));
        config_DeviceDriverInitFuncs[i]();
    }
    for(uint64 i = 0; i < sizeof(config_FSDriverInitFuncs) / sizeof(InitFunc); i++) {
        klog_info("Boot", "Starting FilesystemDriver %i of %i", i+1, sizeof(config_FSDriverInitFuncs) / sizeof(InitFunc));
        config_FSDriverInitFuncs[i]();
    }
    for(uint64 i = 0; i < sizeof(config_ExecInitFuncs) / sizeof(InitFunc); i++) {
        klog_info("Boot", "Starting ExecHandler %i of %i", i+1, sizeof(config_ExecInitFuncs) / sizeof(InitFunc));
        config_ExecInitFuncs[i]();
    }

    VConsoleDriver* vcond = (VConsoleDriver*)DeviceDriverRegistry::GetDriver(config_VConDeviceDriverID);
    uint64 vconid = vcond->AddConsole(&g_TerminalInfo);

    if(g_KernelHeader->ramdiskImage.numPages != 0) {
        RamDeviceDriver* driver = (RamDeviceDriver*)DeviceDriverRegistry::GetDriver(config_RamDeviceDriverID);
        driver->AddDevice((char*)g_KernelHeader->ramdiskImage.buffer, 512, g_KernelHeader->ramdiskImage.numPages * 8);
    }

    kprintf("%C%s\n", 40, 200, 40, config_HelloMessage);

    klog_info("Init", "Mounting boot filesystem");

    DeviceDriver* driver = DeviceDriverRegistry::GetDriver(config_BootFS_DriverID);
    if(driver == nullptr || driver->GetType() != DeviceDriver::TYPE_BLOCK) {
        klog_fatal("Init", "BootFS Driver ID is invalid, aborting boot");
        Scheduler::ThreadExit(1);
    }
    VFS::FileSystem* bootFS = VFS::FileSystemRegistry::CreateFileSystem(config_BootFS_FSID, (BlockDeviceDriver*)driver, config_BootFS_DevID);
    if(bootFS == nullptr) {
        klog_fatal("Init", "Could not create BootFS, aborting boot");
        Scheduler::ThreadExit(1);
    }

    if (config_BootFS_MountToRoot) {
        VFS::Init(bootFS);
    } else {
        VFS::Init(new TempFS());
        VFS::CreateFolder(&g_RootUser, "/boot", { 3, 1, 1 });
        VFS::Mount(&g_RootUser, "/boot", bootFS);

        VFS::CreateFolder(&g_RootUser, "/dev", { 3, 1, 1 });
        VFS::CreateDeviceFile(&g_RootUser, "/dev/tty0", { 3, 1, 1 }, config_VConDeviceDriverID, vconid);
    }

    uint64 tid = SetupInitProcess();

    Scheduler::ThreadExit(0);
}

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    g_KernelHeader = info;

    Terminal::InitTerminalInfo(&g_TerminalInfo, info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth, info->screenColorsInverted);
    Terminal::Clear(&g_TerminalInfo);

    kprintf("%CStarting SimpleOS2 Kernel\n", 40, 200, 40);
    klog_info("Boot", "Kernel at 0x%x", info->kernelImage.buffer);

    MemoryManager::Init(info);
    APIC::Init();
    SMP::GatherInfo(info);
    MemoryManager::InitCore(SMP::GetLogicalCoreID());
    GDT::Init(SMP::GetCoreCount());
    GDT::InitCore(SMP::GetLogicalCoreID());
    IDT::Init();
    IDT::InitCore(SMP::GetLogicalCoreID());
    APIC::InitBootCore();
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