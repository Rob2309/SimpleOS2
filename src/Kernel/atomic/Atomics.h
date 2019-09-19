#pragma once

#include "types.h"

template<typename T>
class Atomic;

template<>
class Atomic<uint64> {
public:
    Atomic() = default;
    Atomic(uint64 val) {
        Write(val);
    }

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
    uint64 PostInc() {
        uint64 res = 1;
        __asm__ __volatile__ (
            "lock xaddq %0, (%1)"
            : "+r"(res)
            : "r"(&m_Value)
        );
        return res;
    }
    Atomic& Dec() {
        __asm__ __volatile__ (
            "lock decq (%0)"
            : : "r"(&m_Value)
        );
        return *this;
    }

    bool DecAndCheckZero() {
        uint64 res = 0;
        __asm__ __volatile__ (
            "lock decq (%1);"
            "cmovzq %2, %0"
            : "+r"(res) : "r"(&m_Value), "r"((uint64)1)
        );
        return res;
    }

    Atomic& operator=(uint64 val) { return Write(val); }
    Atomic& operator++() { return Inc(); }
    uint64 operator++(int) { return PostInc(); }
    Atomic& operator--() { return Dec(); }
    
private:
    volatile uint64 m_Value;
};