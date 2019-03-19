#pragma once

#include "types.h"

class Mutex
{
public:
    Mutex();

    void SpinLock();
    void Unlock();

    bool TryLock();
private:
    volatile uint64 m_Value;
};