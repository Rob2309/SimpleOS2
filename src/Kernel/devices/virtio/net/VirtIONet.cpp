#include "VirtIONetDefines.h"

#include "init/Init.h"
#include "devices/pci/PCI.h"
#include "klib/stdio.h"
#include "arch/port.h"
#include "memory/MemoryManager.h"
#include "klib/memory.h"
#include "scheduler/Scheduler.h"
#include "klib/string.h"

namespace VirtIONet {

    static uint16 g_RegBase;
    static Queue g_ReceiveQueue;
    static Queue g_SendQueue;

    static uint8 g_MAC[6];

    static int64 TestKThread(uint64, uint64);

    static void SetupQueue(int index, Queue& queue) {
        Port::OutWord(g_RegBase + REG_QUEUE_SELECT, index);
        uint16 size = Port::InWord(g_RegBase + REG_QUEUE_SIZE);
        klog_info("VirtIO-Net", "Queue %i has size %i", index, size);

        uint64 bufferDescSize = sizeof(QueueBufferDesc) * size;
        uint64 avlBufferSize = sizeof(QueueAvailableDesc) + sizeof(uint16) * size;
        uint64 usedBufferSize = sizeof(QueueUsedDesc) + sizeof(QueueUsedDescEntry) * size;
        uint64 bufferPages = (bufferDescSize + avlBufferSize + 4095) / 4096 + (usedBufferSize + 4095) / 4096;

        void* buffer = MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages(bufferPages));
        queue.buffers = (QueueBufferDesc*)buffer;
        queue.avlDesc = (QueueAvailableDesc*)((uint64)buffer + bufferDescSize);
        queue.usedDesc = (QueueUsedDesc*)((uint64)buffer + (bufferDescSize + avlBufferSize + 4095) / 4096 * 4096);

        queue.avlDesc->nextIndex = 0;
        queue.usedDesc->nextIndex = 0;

        queue.nextBuffer = 0;

        Port::OutWord(g_RegBase + REG_QUEUE_SELECT, index);
        Port::OutDWord(g_RegBase + REG_QUEUE_ADDR, (uint64)MemoryManager::KernelToPhysPtr(buffer) / 4096);
        Port::OutWord(g_RegBase + REG_QUEUE_NOTIFY, index);

        queue.index = index;
    }

    static void Enqueue(Queue& queue, void* physBuffer, uint64 size, bool send) {
        // add buffer to list
        int buf = queue.nextBuffer;
        queue.nextBuffer += 2;

        queue.buffers[buf].addr = (uint64)physBuffer;
        queue.buffers[buf].flags = QBD_FLAG_LINK;
        queue.buffers[buf].length = sizeof(PacketHeader);
        queue.buffers[buf].next = buf+1;

        queue.buffers[buf+1].addr = (uint64)physBuffer + sizeof(PacketHeader);
        queue.buffers[buf+1].flags = send ? 0 : QBD_FLAG_WRITE;
        queue.buffers[buf+1].length = size - sizeof(PacketHeader);
        queue.buffers[buf+1].next = 0;

        // add buffer to available list
        queue.avlDesc->ring[queue.avlDesc->nextIndex] = buf;
        queue.avlDesc->nextIndex++;

        Port::OutWord(g_RegBase + REG_QUEUE_NOTIFY, queue.index);
    }

    static void Factory(const PCI::Device& dev) {
        uint16 regBase = dev.BARs[0] & 0xFFFC;
        g_RegBase = regBase;

        uint8 flags = FLAG_DEV_ACK;
        Port::OutByte(regBase + REG_DEV_STATUS, flags);
        flags |= FLAG_DRIVER_LOADED;
        Port::OutByte(regBase + REG_DEV_STATUS, flags);

        uint32 features = Port::InDWord(regBase + REG_DEV_FEATURES);
        if((features & F_CSUM) == 0) {
            klog_error("VirtIO-Net", "Controller does not support CSUM");
            return;
        }
        if((features & F_MAC) == 0) {
            klog_error("VirtIO-Net", "Controller does not have a MAC");
            return;
        }
        Port::OutDWord(regBase + REG_DRIVER_FEATURES, F_CSUM | F_MAC);

        flags |= FLAG_FEATURES_OK;
        Port::OutByte(regBase + REG_DEV_STATUS, flags);
        uint8 check = Port::InByte(regBase + REG_DEV_STATUS);
        if(!(check & FLAG_FEATURES_OK)) {
            klog_error("VirtIO-Net", "Requested Featureset unsupported");
            return;
        }

        SetupQueue(0, g_ReceiveQueue);
        SetupQueue(1, g_SendQueue);

        flags |= FLAG_DRIVER_READY;
        Port::OutByte(regBase + REG_DEV_STATUS, flags);

        for(int i = 0; i < 6; i++)
            g_MAC[i] = Port::InByte(regBase + REG_MAC + i);
        klog_info("VirtIO-Net", "MAC: %02X:%02X:%02X:%02X:%02X:%02X", g_MAC[0], g_MAC[1], g_MAC[2], g_MAC[3], g_MAC[4], g_MAC[5]);

        Scheduler::CreateKernelThread(&TestKThread);
    }

    static void Init() {
        PCI::DriverInfo info;
        kmemset(&info, 0xFF, sizeof(info));
        info.vendorID = 0x1AF4;
        info.subsystemID = 1;
        
        PCI::RegisterDriver(info, Factory);
    }
    REGISTER_INIT_FUNC(Init, INIT_STAGE_DEVDRIVERS);

    static void Send(void* physBuffer, uint64 length) {
        Enqueue(g_SendQueue, physBuffer, length, true);
    }

    struct __attribute__((packed)) EthernetHeader {
        uint8 dst[6];
        uint8 src[6];
        uint16 type;
    };
    
    uint16 htons(uint16 val) {
        uint32 b0 = (val >> 0) & 0xFF;
        uint32 b1 = (val >> 8) & 0xFF;

        return (b0 << 8) | (b1);
    }
    uint32 htonl(uint32 val) {
        uint32 b0 = (val >> 0) & 0xFF;
        uint32 b1 = (val >> 8) & 0xFF;
        uint32 b2 = (val >> 16) & 0xFF;
        uint32 b3 = (val >> 24) & 0xFF;

        return (b0 << 24) | (b1 << 16) | (b2 << 8) | (b3);
    }

    static int64 TestKThread(uint64, uint64) {
        klog_info("VirtIO-Net", "Kernelthread started");

        char* packet = (char*)MemoryManager::PhysToKernelPtr(MemoryManager::AllocatePages(1));
        kmemset(packet, 0, 4096);

        auto header = (PacketHeader*)packet;
        header->flags = 0;//PH_FLAG_NEEDS_CSUM;
        header->checksumStart = sizeof(EthernetHeader);
        header->checksumOffset = 10;

        auto etherHeader = (EthernetHeader*)(packet + sizeof(PacketHeader));
        kmemcpy(etherHeader->src, g_MAC, 6);
        uint8 dst[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xAA, 0x55 };
        kmemcpy(etherHeader->dst, dst, 6);
        etherHeader->type = htons(0x1234);

        char* payload = (char*)etherHeader + sizeof(EthernetHeader);
        kstrcpy(payload, "ABCD1234");

        Send(MemoryManager::KernelToPhysPtr(packet), sizeof(PacketHeader) + sizeof(EthernetHeader) + 8);

        while(true) {
            if(g_SendQueue.usedDesc->nextIndex != 0) {
                klog_info("VirtIO-Net", "Sent successfully");
                Scheduler::ThreadExit(0);
            }

            uint8 status = Port::InByte(g_RegBase + REG_DEV_STATUS);
            if(status & FLAG_DEVICE_ERROR) {
                klog_fatal("VirtIO-Net", "Device died");
                Scheduler::ThreadExit(1);
            }
        }
    }

}