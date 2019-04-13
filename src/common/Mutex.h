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
        __asm__ __volatile__ (
            "lock xchgq %%rax, (%0)"
            : : "r"(&m_Value), "a"(0)
        );
    }

    bool TryLock()
    {
        uint64 res;
        __asm__ __volatile__ (
            "lock xchgq %%rax, (%1)"
            : "=a"(res)
            : "r"(&m_Value), "a"(1)
        );
        return res == 0;
    }
private:
    volatile uint64 m_Value;
};