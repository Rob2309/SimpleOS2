#include "PS2.h"
#include "PS2Defines.h"

#include "arch/APIC.h"
#include "arch/IOAPIC.h"
#include "arch/port.h"
#include "klib/stdio.h"

namespace PS2 {

    static void KeyHandler(IDT::Registers* regs) {
        APIC::SignalEOI();

        uint8 scan = Port::InByte(REG_DATA);

        klog_info("PS2", "Scancode %02X received", scan);
    }

    static void WaitForInputBuffer() {
        while(Port::InByte(REG_STATUS) & STATUS_INPUT_BUFFER_FULL) 
            ;
    }

    static void WaitForOutputBuffer() {
        while((Port::InByte(REG_STATUS) & STATUS_OUTPUT_BUFFER_FULL) == 0)
            ;
    }

    void Init() {
        IOAPIC::RegisterIRQ(1, KeyHandler);

        klog_info("PS2", "PS2 keyboard initialized");
    }

}