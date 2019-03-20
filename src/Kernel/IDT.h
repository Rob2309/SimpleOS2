#pragma once

#include "types.h"
#include "ISRNumbers.h"

namespace IDT {
    
    struct __attribute__((packed)) Registers
    {
        uint64 ds;
        uint64 r15, r14, r13, r12, r11, r10, r9, r8;
        uint64 rdi, rsi, rbp, rbx, rdx, rcx, rax;
        uint64 intNumber, errorCode;
        uint64 rip, cs, rflags, userrsp, ss;
    };

    typedef void (*ISR)(Registers* regs);

    void Init();

    void SetISR(uint8 index, ISR isr);
    void SetIST(uint8 index, uint8 ist);

    void EnableInterrupts();
    void DisableInterrupts();

}