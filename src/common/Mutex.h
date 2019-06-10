#pragma once

#include "types.h"

class Mutex
{
public:
    Mutex() : m_Value(0) { }

    void SpinLock()
    {
        while(!TryLock());
    }
    void Unlock()
    {
        uint64 val = 0;
        volatile uint64* loc = &m_Value;
        __asm__ __volatile__ (
            "lock xchgq %0, (%1)"
            : "+r"(val), "+r"(loc)
        );
    }

    bool TryLock()
    {
        uint64 val = 1;
        volatile uint64* loc = &m_Value;
        __asm__ __volatile__ (
            "lock xchgq %0, (%1)"
            : "+r"(val), "+r"(loc)
        );
        return val == 0;
    }
private:
    volatile uint64 m_Value;
};