#pragma once

#include "KernelHeader.h"
#include "interrupts/IDT.h"

namespace IOAPIC {

    void Init(KernelHeader* header);

    void RegisterIRQ(uint8 irq, IDT::ISR isr);

}