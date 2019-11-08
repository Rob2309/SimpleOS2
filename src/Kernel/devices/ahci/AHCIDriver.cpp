#include "AHCIDriver.h"
#include "AHCIDefines.h"

#include "init/Init.h"
#include "devices/pci/PCI.h"
#include "klib/stdio.h"
#include "memory/MemoryManager.h"
#include "devices/DevFS.h"

static AHCIDriver* g_Instance = nullptr;

static void Factory(const PCI::Device& dev);

static void Init() {
    PCI::DriverInfo info;
    kmemset(&info, 0xFF, sizeof(info));
    info.classCode = 0x01;
    info.subclassCode = 0x06;
    info.progIf = 0x01;

    PCI::RegisterDriver(info, Factory);
}
REGISTER_INIT_FUNC(Init, INIT_STAGE_DEVDRIVERS);

static void Factory(const PCI::Device& dev) {
    g_Instance = new AHCIDriver(dev);
}

static void Callback(IDT::Registers* regs) {
    uint32 ports = g_Instance->m_Mem->interruptPendingStatus;

    for(int p = 0; p < 32; p++) {
        if(ports & (1 << p)) {
            volatile HBA_PORT* port = &g_Instance->m_Mem->ports[p];
            SATADisk* disk = &g_Instance->m_Disks[p];

            uint32 ints = port->interruptStatus;
            uint32 ci = port->commandIssue;

            for(int i = 0; i < 32; i++) {
                if(disk->pendingCommands[i].issued && (ci & (1 << i)) == 0) {
                    disk->pendingCommands[i].finishFlag->Write(1);
                    disk->pendingCommands[i].issued = false;
                }
            }

            port->interruptStatus = ints;
        }
    }

    g_Instance->m_Mem->interruptPendingStatus = ports;
}

AHCIDriver::AHCIDriver(const PCI::Device& dev)
    : BlockDeviceDriver("ahci"), m_Dev(dev)
{
    PCI::SetInterruptHandler(m_Dev, Callback);

    m_Mem = (volatile HBA_MEM*)MemoryManager::PhysToKernelPtr((void*)dev.BARs[5]);
    if((m_Mem->capability & HBA_MEM::CAP_64BIT) == 0)
        klog_warning("AHCI", "Controller does not support 64-Bit addressing");

    m_Mem->interruptPendingStatus = m_Mem->interruptPendingStatus;

    for(uint64 i = 0; i < 32; i++) {
        if(m_Mem->portsImplemented & (1 << i)) {
            uint32 signature = m_Mem->ports[i].signature;
            switch(signature) {
            case HBA_PORT::SIGNATURE_ATA: klog_info("AHCI", "SATA Disk on port %i", i); break;
            case HBA_PORT::SIGNATURE_ATAPI: klog_info("AHCI", "SATAPI Disk on port %i", i); break;
            case HBA_PORT::SIGNATURE_EMB: klog_info("AHCI", "Enclosure management bridge on port %i", i); break;
            case HBA_PORT::SIGNATURE_PM: klog_info("AHCI", "Port Multiplier on port %i", i); break;
            default: klog_warning("AHCI", "Unknown device on port %i", i); break;
            }

            m_Disks[i].present = true;

            void* cmdListBuffer = MemoryManager::AllocatePages(NUM_PAGES(1024));
            m_Disks[i].commandListBuffer = MemoryManager::PhysToKernelPtr(cmdListBuffer);
            kmemset(m_Disks[i].commandListBuffer, 0, NUM_PAGES(1024) * 4096);

            void* fisBuffer = MemoryManager::AllocatePages(NUM_PAGES(256));
            m_Disks[i].fisBuffer = MemoryManager::PhysToKernelPtr(fisBuffer);
            kmemset(m_Disks[i].fisBuffer, 0, NUM_PAGES(256) * 4096);

            void* cmdTableBuffer = MemoryManager::AllocatePages(NUM_PAGES((sizeof(HBA_CMD) + sizeof(HBA_PRDT_ENTRY) * 8) * 32));
            m_Disks[i].commandTableBuffer = MemoryManager::PhysToKernelPtr(cmdTableBuffer);
            kmemset(m_Disks[i].commandTableBuffer, 0, NUM_PAGES((sizeof(HBA_CMD) + sizeof(HBA_PRDT_ENTRY) * 8) * 32) * 4096);

            for(int c = 0; c < 32; c++)
                m_Disks[i].pendingCommands[c].issued = false;

            SetupPort(i, cmdListBuffer, fisBuffer, cmdTableBuffer);

            DevFS::RegisterBlockDevice("ahci0", GetDriverID(), i);
        }
    }

    m_Mem->globalHostControl |= HBA_MEM::GHC_INT_ENABLE;
}

int64 AHCIDriver::DeviceCommand(uint64 subID, int64 command, void* arg) {
    return OK;
}

uint64 AHCIDriver::GetBlockSize(uint64 subID) const {
    return 512;
}

void AHCIDriver::ScheduleOperation(uint64 subID, uint64 startBlock, uint64 numBlocks, bool write, void* buffer, Atomic<uint64>* finishFlag) {
    if(write) {
        *finishFlag = true;
    } else {
        ScheduleRead(subID, startBlock, numBlocks, buffer, finishFlag);
    }
}

static int FindCmdSlot(volatile HBA_PORT* port) {
    for(int i = 0; i < 32; i++) {
        if((port->commandIssue & (1 << i)) == 0 && (port->sataActive & (1 << i)) == 0 && (port->interruptStatus & (1 << i)) == 0)
            return i;
    }
    return -1;
}

void AHCIDriver::ScheduleRead(int portIndex, uint64 startBlock, uint64 numBlocks, void* kernelBuffer, Atomic<uint64>* finishFlag) {
    void* physBuffer = MemoryManager::KernelToPhysPtr(kernelBuffer);

    volatile HBA_PORT* port = &m_Mem->ports[portIndex];
    SATADisk* portInfo = &m_Disks[portIndex];

    int slot;
    while((slot = FindCmdSlot(port)) == -1) ;

    HBA_CMD* cmd = (HBA_CMD*)((uint64)portInfo->commandTableBuffer + (sizeof(HBA_CMD) + sizeof(HBA_PRDT_ENTRY) * 8) * slot);
    kmemset(cmd, 0, sizeof(HBA_CMD) + sizeof(HBA_PRDT_ENTRY) * 8);
    
    FIS_REG_H2D* fis = (FIS_REG_H2D*)(cmd->cmdFIS);
    fis->fisType = FIS_TYPE_REG_H2D;
    fis->pmt = FIS_REG_H2D::CMD_MASK;
    fis->command = 0x25;
    fis->lba0 = startBlock;
    fis->lba1 = startBlock >> 8;
    fis->lba2 = startBlock >> 16;
    fis->device = 1 << 6;
    fis->lba3 = startBlock >> 24;
    fis->lba4 = startBlock >> 32;
    fis->lba5 = startBlock >> 40;
    fis->count = numBlocks;

    cmd->prdt[0].dataBaseAddressLow = (uint64)physBuffer & 0xFFFFFFFF;
    cmd->prdt[0].dataBaseAddressHigh = (uint64)physBuffer >> 32;
    cmd->prdt[0].dataCountAndFlags = (numBlocks * 512 - 1);
    cmd->prdt[0].dataCountAndFlags |= HBA_PRDT_ENTRY::FLAGS_INT_ON_COMPLETE;

    HBA_CMD_HEADER* cmdHeader = (HBA_CMD_HEADER*)((uint64)portInfo->commandListBuffer + sizeof(HBA_CMD_HEADER) * slot);
    kmemset(cmdHeader, 0, sizeof(HBA_CMD_HEADER));
    cmdHeader->prdtLength = 1;
    uint64 cmdTableAddr = (uint64)MemoryManager::KernelToPhysPtr(cmd);
    cmdHeader->cmdTableAddrLow = cmdTableAddr & 0xFFFFFFFF;
    cmdHeader->cmdTableAddrHigh = cmdTableAddr >> 32;
    cmdHeader->descriptionInfo = HBA_CMD_HEADER::DI_SET_COMMAND_LENGTH(sizeof(FIS_REG_H2D) / sizeof(uint32));

    while(port->taskFileData & 0x88) ;

    port->commandIssue = (1 << slot);

    portInfo->pendingCommands[slot].issued = true;
    portInfo->pendingCommands[slot].finishFlag = finishFlag;
}

void AHCIDriver::StopPort(int portIndex) {
    auto& port = m_Mem->ports[portIndex];

    port.cmdAndStatus &= ~HBA_PORT::CONTROL_START;

    while(true) {
        if(port.cmdAndStatus & HBA_PORT::STATUS_FIS_RUNNING)
            continue;
        if(port.cmdAndStatus & HBA_PORT::STATUS_CMDLIST_RUNNING)
            continue;
        break;
    }

    port.cmdAndStatus &= ~HBA_PORT::CONTROL_ENABLE_FIS_RECEIVE;
}

void AHCIDriver::StartPort(int portIndex) {
    auto& port = m_Mem->ports[portIndex];

    while(port.cmdAndStatus & HBA_PORT::STATUS_CMDLIST_RUNNING) ;

    port.cmdAndStatus |= HBA_PORT::CONTROL_ENABLE_FIS_RECEIVE;
    port.cmdAndStatus |= HBA_PORT::CONTROL_START;
}

void AHCIDriver::SetupPort(int portIndex, void* cmdListBuffer, void* fisBuffer, void* cmdTableBuffer) {
    auto& port = m_Mem->ports[portIndex];
    auto& portInfo = m_Disks[portIndex];

    StopPort(portIndex);

    port.commandListBaseLow = (uint64)cmdListBuffer & 0xFFFFFFFF;
    port.commandListBaseHigh = ((uint64)cmdListBuffer >> 32) & 0xFFFFFFFF;

    port.fisBaseLow = (uint64)fisBuffer & 0xFFFFFFFF;
    port.fisBaseHigh = ((uint64)fisBuffer >> 32) & 0xFFFFFFFF;

    auto cmd = (HBA_CMD_HEADER*)portInfo.commandListBuffer;
    for(int i = 0; i < 32; i++) {
        cmd[i].prdtLength = 1;
        uint64 cmdTableAddr = (uint64)cmdTableBuffer + i * (sizeof(HBA_CMD) + sizeof(HBA_PRDT_ENTRY) * 8);
        cmd[i].cmdTableAddrLow = cmdTableAddr & 0xFFFFFFFF;
        cmd[i].cmdTableAddrHigh = (cmdTableAddr >> 32) & 0xFFFFFFFF;
    }

    port.interruptStatus = port.interruptStatus;
    port.interruptEnable = 0xFFFFFFFF;

    StartPort(portIndex);
}