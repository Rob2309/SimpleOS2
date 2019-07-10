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
#include "fs/TestFS.h"
#include "devices/RamDeviceDriver.h"
#include "fs/ext2/ext2.h"

#include "scheduler/Scheduler.h"
#include "scheduler/ELF.h"
#include "user/User.h"
#include "klib/string.h"

static User g_RootUser;
static User g_TestUser;

static uint64 SetupTestProcess() {
    uint64 file;
    int64 error = VFS::Open(&g_RootUser, "/initrd/Test.elf", VFS::Permissions::Read, file);
    if(error != VFS::OK) {
        klog_error("Test", "Failed to open /initrd/Test.elf");
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
        klog_error("Test", "Failed to setup test process");
        delete[] buffer;
        return 0;
    }

    delete[] buffer;

    uint64 tid = Scheduler::CreateUserThread(pml4Entry, &regs, &g_TestUser);
    klog_info("Test", "Created test process with TID %i", tid);
    return tid;
}

static VFS::FileSystem* TestFSFactory() {
    return new TestFS();
}

static KernelHeader* g_KernelHeader;

static void InitThread() {
    klog_info("Init", "Init Thread starting");

    g_RootUser.gid = 0;
    g_RootUser.uid = 0;
    kstrcpy(g_RootUser.name, "root");

    g_TestUser.gid = 123;
    g_TestUser.uid = 456;
    kstrcpy(g_TestUser.name, "TestUser");

    VFS::FileSystemRegistry::RegisterFileSystem("test", TestFSFactory);

    VFS::Init(new TestFS());
    VFS::CreateFolder(&g_RootUser, "/dev", { VFS::Permissions::Read | VFS::Permissions::Write, VFS::Permissions::Read, VFS::Permissions::Read });
    VFS::CreateFolder(&g_RootUser, "/initrd", { VFS::Permissions::Read | VFS::Permissions::Write, VFS::Permissions::Read, VFS::Permissions::Read });

    auto ramdriver = new RamDeviceDriver();
    uint64 initrdID = ramdriver->AddDevice((char*)g_KernelHeader->ramdiskImage.buffer, 512, g_KernelHeader->ramdiskImage.numPages * 8);
    VFS::CreateDeviceFile(&g_RootUser, "/dev/ram0", { VFS::Permissions::Read, VFS::Permissions::Read, VFS::Permissions::Read }, ramdriver->GetDriverID(), initrdID);

    Ext2::Init();
    VFS::Mount(&g_RootUser, "/initrd", "ext2", "/dev/ram0");

    uint64 tid = SetupTestProcess();

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