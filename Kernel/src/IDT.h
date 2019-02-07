#pragma once

#include "types.h"
#include "ISRNumbers.h"

namespace IDT {
    
    struct __attribute__((packed)) Registers
    {
        uint64 ds;
        uint64 rdi, rsi, rbp, rsp, rbx, rdx, rcx, rax;
        uint64 intNumber, errorCode;
        uint64 rip, cs, rflags, userrsp, ss;
    };

    typedef void (*ISR)(Registers* regs);

    void Init();

    void SetISR(uint8 index, ISR isr);

    void EnableInterrupts();
    void DisableInterrupts();

}