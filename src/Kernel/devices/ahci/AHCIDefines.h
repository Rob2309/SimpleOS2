#pragma once

#include "types.h"

constexpr uint8 FIS_TYPE_REG_H2D = 0x27;
constexpr uint8 FIS_TYPE_REG_D2H = 0x34;
constexpr uint8 FIS_TYPE_DMA_ACT = 0x39;
constexpr uint8 FIS_TYPE_DMA_SETUP = 0x41;
constexpr uint8 FIS_TYPE_DATA = 0x46;
constexpr uint8 FIS_TYPE_BIST = 0x58;
constexpr uint8 FIS_TYPE_PIO_SETUP = 0x5F;
constexpr uint8 FIS_TYPE_DEV_BITS = 0xA1;

struct __attribute__((packed)) FIS_REG_H2D {
    uint8 fisType;

    uint8 portMultiplier : 4;
    uint8 reserved0 : 3;
    uint8 c : 1;

    uint8 command;
    uint8 featuresLow;
    uint8 lba0;
    uint8 lba1;
    uint8 lba2;
    uint8 device;
    uint8 lba3;
    uint8 lba4;
    uint8 lba5;
    uint8 featuresHigh;
    uint16 count;
    uint8 icc;
    uint8 control;
    uint32 reserved1;
};

struct __attribute__((packed)) FIS_REG_D2H {
    uint8 fisType;

    uint8 portMultiplier : 4;
    uint8 reserved0 : 2;
    uint8 interrupt : 1;
    uint8 reserved1 : 1;

    uint8 status;
    uint8 error;
    uint8 lba0;
    uint8 lba1;
    uint8 lba2;
    uint8 device;
    uint8 lba3;
    uint8 lba4;
    uint8 lba5;
    uint8 reserved2;
    uint16 count;
    uint16 reserved3;
    uint32 reserved4;
};

struct __attribute__((packed)) FIS_DATA {
    uint8 fisType;

    uint8 portMultiplier : 4;
    uint8 reserved0 : 4;

    uint16 reserved1;
    uint32 data[];
};

struct __attribute__((packed)) FIS_PIO_SETUP {
    uint8 fisType;

    uint8 portMultiplier : 4;
    uint8 reserved0 : 1;
    uint8 direction : 1;
    uint8 interrupt : 1;
    uint8 reserved1 : 1;

    uint8 status;
    uint8 error;
    uint8 lba0;
    uint8 lba1;
    uint8 lba2;
    uint8 device;
    uint8 lba3;
    uint8 lba4;
    uint8 lba5;
    uint8 reserved2;
    uint16 count;
    uint8 reserved3;
    uint8 newStatus;
    uint16 tc;
    uint32 reserved4;
};

struct __attribute__((packed)) FIS_DMA_SETUP {
    uint8 fisType;

    uint8 portMultiplier : 4;
    uint8 reserved0 : 1;
    uint8 direction : 1;
    uint8 interrupt : 1;
    uint8 autoActivate : 1;

    uint16 reserved1;
    uint64 dmaBufferID;
    uint32 reserved2;
    uint32 dmaBufferOffs;
    uint32 transferCount;
    uint32 reserved3;
};

struct __attribute__((packed)) HBA_PORT {
    constexpr static uint32 SIGNATURE_ATA = 0x00000101;
    constexpr static uint32 SIGNATURE_ATAPI = 0xEB140101;
    constexpr static uint32 SIGNATURE_EMB = 0xC33C0101;
    constexpr static uint32 SIGNATURE_PM = 0x96690101;

    constexpr static uint32 INT_COLD_PORT = (1 << 31);
    constexpr static uint32 INT_TASK_FILE_ERROR = (1 << 30);
    constexpr static uint32 INT_HOST_FATAL_ERROR = (1 << 29);
    constexpr static uint32 INT_HOST_DATA_ERROR = (1 << 28);
    constexpr static uint32 INT_INTERFACE_FATAL_ERROR = (1 << 27);
    constexpr static uint32 INT_INTERFACE_ERROR = (1 << 26);
    constexpr static uint32 INT_OVERFLOW = (1 << 24);
    constexpr static uint32 INT_INVALID_PM_STATUS = (1 << 23);
    constexpr static uint32 INT_PHYSRDY_CHANGE_STATUS = (1 << 22);
    constexpr static uint32 INT_DEV_MECH_PRESENCE_STATUS = (1 << 7);
    constexpr static uint32 INT_PORT_CONNECTION_CHANGE = (1 << 6);
    constexpr static uint32 INT_DESCRIPTOR_PROCESSED = (1 << 5);
    constexpr static uint32 INT_UNKNOWN_FIS = (1 << 4);
    constexpr static uint32 INT_SETDEVBITS = (1 << 3);
    constexpr static uint32 INT_DMA_SETUP = (1 << 2);
    constexpr static uint32 INT_PIO_SETUP = (1 << 1);
    constexpr static uint32 INT_D2H_REG = (1 << 0);

    constexpr static uint32 STATUS_CMDLIST_RUNNING = (1 << 15);
    constexpr static uint32 STATUS_FIS_RUNNING = (1 << 14);

    constexpr static uint32 CONTROL_ENABLE_FIS_RECEIVE = (1 << 4);
    constexpr static uint32 CONTROL_START = (1 << 0);

    uint32 commandListBaseLow;
    uint32 commandListBaseHigh;
    uint32 fisBaseLow;
    uint32 fisBaseHigh;
    uint32 interruptStatus;
    uint32 interruptEnable;
    uint32 cmdAndStatus;
    uint32 reserved;
    uint32 taskFileData;
    uint32 signature;
    uint32 sataStatus;
    uint32 sataControl;
    uint32 sataError;
    uint32 sataActive;
    uint32 commandIssue;
    uint32 sataNotification;
    uint32 fisSwitchControl;
    uint32 reserved2[11];
    uint32 vendor[4];
};

struct __attribute__((packed)) HBA_MEM {
    constexpr static uint32 CAP_64BIT = (1 << 31);
    constexpr static uint32 CAP_NCQ = (1 << 30);
    constexpr static uint32 CAP_NUM_CMD_SLOTS(uint32 cap) { return (cap >> 8) & 0x1F; }

    constexpr static uint32 GHC_AHCI_MODE = (1 << 31);
    constexpr static uint32 GHC_INT_ENABLE = (1 << 1);
    constexpr static uint32 GHC_RESET = (1 << 0);

    constexpr static uint32 CCC_CONTROL_GET_TIMEOUT(uint32 cccc) { return (cccc >> 16) & 0xFFFF; }
    constexpr static uint32 CCC_CONTROL_SET_TIMEOUT(uint32 to) { return (to << 16); }
    constexpr static uint32 CCC_CONTROL_GET_CMDC(uint32 cccc) { return (cccc >> 8) & 0xF; }
    constexpr static uint32 CCC_CONTROL_SET_CMDC(uint32 cmdc) { return (cmdc << 8); }
    constexpr static uint32 CCC_CONTROL_GET_INT(uint32 cccc) { return (cccc >> 3) & 0x1F; }
    constexpr static uint32 CCC_CONTROL_SET_INT(uint32 intNo) { return (intNo << 3); }
    constexpr static uint32 CCC_CONTROL_ENABLE = 0x1;

    uint32 capability;
    uint32 globalHostControl;
    uint32 interruptPendingStatus;
    uint32 portsImplemented;
    uint32 version;
    uint32 commandCompletionCoalescingControl;
    uint32 commandCompletionCoalescingPorts;
    uint32 enclosureManagementLoc;
    uint32 enclosureManagementCtrl;
    uint32 capability2;
    uint32 handoffControlAndStatus;
    uint8 reserved[116];
    uint8 vendor[96];

    HBA_PORT ports[];
};

struct __attribute__((packed)) HBA_CMD_HEADER {
    constexpr static uint16 DI_SET_PM(uint32 pm) { return (pm << 12); }
    constexpr static uint16 DI_GET_PM(uint32 di) { return (di >> 12) & 0xF; }
    constexpr static uint16 DI_CLEAR_BUSY = (1 << 10);
    constexpr static uint16 DI_BIST = (1 << 9);
    constexpr static uint16 DI_RESET = (1 << 8);
    constexpr static uint16 DI_PREFETCHABLE = (1 << 7);
    constexpr static uint16 DI_WRITE = (1 << 6);
    constexpr static uint16 DI_ATAPI = (1 << 5);
    constexpr static uint16 DI_SET_COMMAND_LENGTH(uint32 l) { return l; }
    constexpr static uint16 DI_GET_COMMAND_LENGTH(uint32 di) { return di & 0x1F; }

    uint16 descriptionInfo;
    uint16 prdtLength;

    uint32 bytesTransferred;
    uint32 cmdTableAddrLow;
    uint32 cmdTableAddrHigh;
    uint32 reserved[4];
};

struct __attribute((packed)) HBA_PRDT_ENTRY {
    constexpr static uint32 FLAGS_INT_ON_COMPLETE = (1 << 31);

    uint32 dataBaseAddressLow;
    uint32 dataBaseAddressHigh;
    uint32 reserved;
    uint32 dataCountAndFlags;
};

struct __attribute__((packed)) HBA_CMD {
    char cmdFIS[64];
    char atapiCmd[16];
    char reserved[48];
    HBA_PRDT_ENTRY prdt[];
};