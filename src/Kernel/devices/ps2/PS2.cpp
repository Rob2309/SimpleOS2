#include "PS2.h"
#include "PS2Defines.h"

#include "arch/APIC.h"
#include "arch/IOAPIC.h"
#include "arch/port.h"
#include "klib/stdio.h"

#include "errno.h"

#include "init/Init.h"

#include "devices/DevFS.h"

namespace PS2 {

    static StickyLock g_BufferLock;
    static uint8 g_Buffer[1024];
    static uint64 g_ReadIndex = 0;
    static uint64 g_WriteIndex = 0;
    static bool g_LeftShift = false;
    static bool g_RightShift = false;

    static void KeyHandler(IDT::Registers* regs) {
        uint8 scan = Port::InByte(REG_DATA);

        if(scan == 0x2A) {
            g_LeftShift = true;
            return;
        }
        if(scan == 0x36) {
            g_RightShift = true;
            return;
        }
        if(scan == 0xAA) {
            g_LeftShift = false;
            return;
        }
        if(scan == 0xB6) {
            g_RightShift = false;
            return;
        }

        if(scan & 0x80)
            return;

        char c = (g_LeftShift || g_RightShift) ? g_ScanMapShift[scan] : g_ScanMap[scan];
        if(c == '\0')
            return;

        //g_BufferLock.Spinlock_Raw();
        g_Buffer[g_WriteIndex++ % sizeof(g_Buffer)] = c;
        //g_BufferLock.Unlock_Raw();
    }

    PS2Driver::PS2Driver()
        : CharDeviceDriver("ps2")
    {
        IOAPIC::RegisterIRQ(1, KeyHandler);

        DevFS::RegisterCharDevice("keyboard", GetDriverID(), DeviceKeyboard);
    }

    uint64 PS2Driver::Read(uint64 subID, void* buffer, uint64 bufferSize) {
        //g_BufferLock.Spinlock_Cli();

        uint64 rem = g_WriteIndex - g_ReadIndex;
        if(rem > bufferSize)
            rem = bufferSize;

        for(uint64 i = 0; i < rem; i++) {
            ((char*)buffer)[i] = g_Buffer[(g_ReadIndex + i) % sizeof(g_Buffer)];
        }

        g_ReadIndex += rem;
        //g_BufferLock.Unlock_Cli();
        return rem;
    }

    uint64 PS2Driver::Write(uint64 subID, const void* buffer, uint64 bufferSize) {
        return bufferSize;
    }

    static void Init() {
        new PS2Driver();
    }
    REGISTER_INIT_FUNC(Init, INIT_STAGE_DEVDRIVERS);

}