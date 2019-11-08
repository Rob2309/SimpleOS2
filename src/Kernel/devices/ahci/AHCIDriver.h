#pragma once

#include "../DeviceDriver.h"
#include "devices/pci/PCI.h"

#include "AHCIDefines.h"

struct SATADisk {
    bool present;
    void* commandListBuffer;
    void* fisBuffer;
    void* commandTableBuffer;

    struct {
        bool issued;
        Atomic<uint64>* finishFlag;
    } pendingCommands[32];
};

class AHCIDriver : public BlockDeviceDriver {
public:
    AHCIDriver(const PCI::Device& dev);

    virtual uint64 GetBlockSize(uint64 subID) const override;

    virtual int64 DeviceCommand(uint64 subID, int64 command, void* arg) override;

protected:
    virtual void ScheduleOperation(uint64 subID, uint64 startBlock, uint64 numBlocks, bool write, void* buffer, Atomic<uint64>* finishFlag) override;

private:
    void StopPort(int portIndex);
    void StartPort(int portIndex);
    void SetupPort(int portIndex, void* cmdListBuffer, void* fisBuffer, void* cmdTableBuffer);

    void ScheduleRead(int portIndex, uint64 startBlock, uint64 numBlocks, void* kernelBuffer, Atomic<uint64>* finishFlag);

public:
    const PCI::Device& m_Dev;
    volatile HBA_MEM* m_Mem;
    SATADisk m_Disks[32];
};