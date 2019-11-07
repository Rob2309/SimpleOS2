#pragma once

#include "types.h"

namespace VirtIONet {

    #define QBD_FLAG_LINK 1
    #define QBD_FLAG_WRITE 2
    #define QBD_FLAG_INDIRECT 4

    struct __attribute__((packed)) QueueBufferDesc {
        uint64 addr;
        uint32 length;
        uint16 flags;
        uint16 next;
    };

    struct __attribute__((packed)) QueueAvailableDesc {
        uint16 intDisable;
        uint16 nextIndex;
        uint16 ring[];
    };

    struct __attribute__((packed)) QueueUsedDescEntry {
        uint32 index;
        uint32 length;
    };
    struct __attribute__((packed)) QueueUsedDesc {
        uint16 intDisable;
        uint16 nextIndex;
        QueueUsedDescEntry entries[];
    };

    #define PH_FLAG_NEEDS_CSUM 1
    #define PH_FLAG_DATA_VALID 2
    #define PH_FLAG_RSC_INFO 4
    #define PH_GSO_NONE 0
    #define PH_GSO_TCPV4 1
    #define PH_GSO_UDP 3
    #define PH_GSO_TCPV6 4
    #define PH_GSO_ECN 0x80
    struct __attribute__((packed)) PacketHeader {
        uint8 flags;
        uint8 gsoType;
        uint16 headerLength;
        uint16 gsoSize;
        uint16 checksumStart;
        uint16 checksumOffset;
        //uint16 numBuffers; // only present if F_MRG_RXBUF negotiated
    };

    #define REG_DEV_FEATURES 0x00
    #define REG_DRIVER_FEATURES 0x04
    #define REG_QUEUE_ADDR 0x08
    #define REG_QUEUE_SIZE 0x0C
    #define REG_QUEUE_SELECT 0x0E
    #define REG_QUEUE_NOTIFY 0x10
    #define REG_DEV_STATUS 0x12
    #define REG_ISR_STATUS 0x13
    #define REG_MAC 0x14
    #define REG_STATUS 0x1A

    #define FLAG_DEV_ACK 0x01
    #define FLAG_DRIVER_LOADED 0x02
    #define FLAG_DRIVER_READY 0x04
    #define FLAG_FEATURES_OK 0x08
    #define FLAG_DEVICE_ERROR 0x40
    #define FLAG_DRIVER_FAILED 0x80

    struct Queue {
        int index;

        QueueBufferDesc* buffers;
        QueueAvailableDesc* avlDesc;
        QueueUsedDesc* usedDesc;

        int nextBuffer;
    };

    #define F_CSUM (1 << 0)
    #define F_MAC (1 << 5)

}