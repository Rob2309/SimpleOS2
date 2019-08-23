#pragma once

#include "KernelHeader.h"
#include "interrupts/IDT.h"

namespace IOAPIC {

    void Init();

    void RegisterIRQ(uint8 irq, IDT::ISR isr);
    void RegisterGSI(uint32 gsi, IDT::ISR isr);

}