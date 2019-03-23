#pragma once

#define WriteMSR(number, value)             \
    __asm__ __volatile__ (                  \
        "wrmsr"                             \
        : :                                 \
        "rcx"(number),                      \
        "rdx"((value >> 32) & 0xFFFFFFFF),  \
        "rax"(value & 0xFFFFFFFF)           \
    );