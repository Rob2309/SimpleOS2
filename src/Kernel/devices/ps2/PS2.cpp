#include "PS2.h"
#include "PS2Defines.h"

#include "arch/APIC.h"
#include "arch/IOAPIC.h"
#include "arch/port.h"
#include "klib/stdio.h"

#include "errno.h"

#include "init/Init.h"

#include "devices/DevFS.h"

#include "PS2Keys.h"

namespace PS2 {

    constexpr uint64 QUEUE_SIZE = 128;

    static StickyLock g_QueueLock;
    static uint16 g_Queue[QUEUE_SIZE];
    static uint64 g_ReadIndex = 0;
    static uint64 g_WriteIndex = 0;

    static bool g_E0Key = false;

    static void WriteKey(uint16 key) {
        g_QueueLock.Spinlock_Raw();
        if(g_WriteIndex - g_ReadIndex < QUEUE_SIZE)
            g_Queue[g_WriteIndex++ % QUEUE_SIZE] = key;
        g_QueueLock.Unlock_Raw();
    }

    static void KeyHandler(IDT::Registers* regs) {
        uint8 scan = Port::InByte(REG_DATA);

        if(g_E0Key) {
            if(scan == 0x48) {
                WriteKey(KEY_UP);
            } else if(scan == 0x50) {
                WriteKey(KEY_DOWN);     
            }

            g_E0Key = false;
        }

        if(scan == 0x2A) {
            WriteKey(KEY_SHIFT_LEFT);
            return;
        }
        if(scan == 0x36) {
            WriteKey(KEY_SHIFT_RIGHT);
            return;
        }
        if(scan == 0xAA) {
            WriteKey(KEY_SHIFT_LEFT | KEY_RELEASED);
            return;
        }
        if(scan == 0xB6) {
            WriteKey(KEY_SHIFT_RIGHT | KEY_RELEASED);
            return;
        }
        if(scan == 0x1D) {
            WriteKey(KEY_CTRL_LEFT);
            return;
        }
        if(scan == 0x9D) {
            WriteKey(KEY_CTRL_LEFT | KEY_RELEASED);
            return;
        }

        if(scan == 0xE0) {
            g_E0Key = true;
            return;
        } 
        
        uint16 key = 0;
        if(scan & 0x80) {
            key |= KEY_RELEASED;
            scan &= 0x7F;
        }

        uint16 k = g_ScanMap[scan];
        if(k == '\0')
            return;

        key |= k;

        WriteKey(key);
    }

    PS2Driver::PS2Driver()
        : CharDeviceDriver("ps2")
    {
        IOAPIC::RegisterIRQ(1, KeyHandler);

        DevFS::RegisterCharDevice("keyboard", GetDriverID(), DeviceKeyboard);
    }

    int64 PS2Driver::DeviceCommand(uint64 subID, int64 command, void* arg) {
        return OK;
    }

    uint64 PS2Driver::Read(uint64 subID, void* buffer, uint64 bufferSize) {
        g_QueueLock.Spinlock_Cli();

        if(g_ReadIndex > g_WriteIndex) {
            klog_error("PS2", "Buffer underflow");
        }

        uint64 rem = g_WriteIndex - g_ReadIndex;
        if(rem > bufferSize / 2)
            rem = bufferSize / 2;

        for(uint64 i = 0; i < rem; i++) {
            ((uint16*)buffer)[i] = g_Queue[(g_ReadIndex + i) % QUEUE_SIZE];
        }

        g_ReadIndex += rem;
        g_QueueLock.Unlock_Cli();
        return rem * 2;
    }

    uint64 PS2Driver::Write(uint64 subID, const void* buffer, uint64 bufferSize) {
        return ErrorPermissionDenied;
    }

    static void Init() {
        new PS2Driver();
    }
    REGISTER_INIT_FUNC(Init, INIT_STAGE_DEVDRIVERS);

}