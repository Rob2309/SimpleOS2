#pragma once

#include "types.h"

class StickyLock {
public:
    StickyLock();

    void SpinLock();
    void Unlock();

    void SpinLock_NoSticky();
    void Unlock_NoSticky();

private:
    bool TryLock();
    void DoUnlock();

private:
    uint64 m_Val;
};