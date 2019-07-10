#pragma once

#include "types.h"

class StickyLock {
public:
    StickyLock();

    void Spinlock();
    void Unlock();

    void Spinlock_Cli();
    void Unlock_Cli();

    void Spinlock_Raw();
    void Unlock_Raw();

private:
    bool TryLock();
    void DoUnlock();

private:
    uint64 m_Val;
};