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
#include "scheduler/ELF.h"
#include "user/User.h"
#include "klib/string.h"

#include "Config.h"

static User g_RootUser;

static uint64 SetupInitProcess() {
    uint64 file;
    int64 error = VFS::Open(&g_RootUser, config_Init_Command, VFS::Permissions::Read, file);
    if(error != VFS::OK) {
        klog_fatal("Init", "Failed to open %s (%s), aborting boot", config_Init_Command, VFS::ErrorToString(error));
        return 0;
    }
    
    VFS::NodeStats stats;
    VFS::Stat(file, &stats);
    
    uint8* buffer = new uint8[stats.size];
    VFS::Read(file, buffer, stats.size);
    VFS::Close(file);

    uint64 pml4Entry;
    IDT::Registers regs;
    if(!PrepareELF(buffer, pml4Entry, regs)) {
        klog_error("Init", "Failed to setup init process, aborting boot");
        delete[] buffer;
        return 0;
    }

    delete[] buffer;

    uint64 tid = Scheduler::CreateUserThread(pml4Entry, &regs, &g_RootUser);
    klog_info("Init", "Created init process with TID %i", tid);
    return tid;
}

static KernelHeader* g_KernelHeader;

static void InitThread() {
    klog_info("Init", "Init Thread starting");

    g_RootUser.gid = 0;
    g_RootUser.uid = 0;
    kstrcpy(g_RootUser.name, "root");

    for(uint64 i = 0; i < sizeof(config_DeviceDriverInitFuncs) / sizeof(InitFunc); i++) {
        config_DeviceDriverInitFuncs[i]();
    }
    for(uint64 i = 0; i < sizeof(config_FSDriverInitFuncs) / sizeof(InitFunc); i++) {
        config_FSDriverInitFuncs[i]();
    }

    if(g_KernelHeader->ramdiskImage.numPages != 0) {
        RamDeviceDriver* driver = (RamDeviceDriver*)DeviceDriverRegistry::GetDriver(config_RamDeviceDriverID);
        driver->AddDevice((char*)g_KernelHeader->ramdiskImage.buffer, 512, g_KernelHeader->ramdiskImage.numPages * 8);
    }

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

    if constexpr (config_BootFS_MountToRoot) {
        VFS::Init(bootFS);
    } else {
        VFS::Init(new TempFS());
        VFS::CreateFolder(&g_RootUser, "/boot", { 3, 1, 1 });
        VFS::Mount(&g_RootUser, "/boot", bootFS);
    }

    uint64 tid = SetupInitProcess();

    Scheduler::ThreadExit(0);
}

extern "C" void __attribute__((noreturn)) main(KernelHeader* info) {
    g_KernelHeader = info;

    Terminal::Init(info->screenBuffer, info->screenWidth, info->screenHeight, info->screenScanlineWidth, info->screenColorsInverted);
    Terminal::Clear();

    klog_info("Kernel", "Kernel at 0x%x", info->kernelImage.buffer);

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

    Scheduler::Init(SMP::GetCoreCount());

    SMP::StartCores();
    MemoryManager::EarlyFreePages(MemoryManager::KernelToPhysPtr(info->smpTrampolineBuffer), info->smpTrampolineBufferPages);

    Scheduler::CreateInitThread(InitThread);
    
    SMP::StartSchedulers();
    Scheduler::Start();

    while(true) ;
}