#include "Mutex.h"

Mutex::Mutex()
    : m_Value(1)
{ }

void Mutex::SpinLock()
{
    while(!TryLock());
}
void Mutex::Unlock()
{
    __asm__ __volatile__ (
        "lock xchg %%rax, (%0)"
        : : "r"(&m_Value), "a"(1)
    );
}

bool Mutex::TryLock()
{
    uint64 res;
    __asm__ __volatile__ (
        "lock xchgq %%rax, (%1)"
        : "=a"(res)
        : "r"(&m_Value), "a"(0)
    );
    return res == 1;
}