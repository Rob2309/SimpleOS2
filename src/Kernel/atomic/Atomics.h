#pragma once

#include "types.h"

template<typename T>
class Atomic;

template<>
class Atomic<uint64> {
public:
    Atomic& Write(uint64 val) {
        __asm__ __volatile__ (
            "movq %0, (%1)"
            : : "r"(val), "r"(&m_Value)
        );
        return *this;
    }
    uint64 Read() const {
        uint64 ret;
        __asm__ __volatile__ (
            "movq (%1), %0"
            : "=r"(ret)
            : "r" (&m_Value)
        );
        return ret;
    }
    Atomic& Inc() {
        __asm__ __volatile__ (
            "lock incq (%0)"
            : : "r"(&m_Value)
        );
        return *this;
    }
    Atomic& Dec() {
        __asm__ __volatile__ (
            "lock decq (%0)"
            : : "r"(&m_Value)
        );
        return *this;
    }

    Atomic& operator=(uint64 val) { return Write(val); }
    Atomic& operator++() { return Inc(); }
    Atomic& operator--() { return Dec(); }
    
private:
    volatile uint64 m_Value;
};