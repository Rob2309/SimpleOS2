#pragma once

#include "types.h"

/**
 * This lock is a classical spinlock.
 **/
class StickyLock {
public:
    StickyLock();

    /**
     * Try to lock the StickyLock. 
     * The calling Thread will not be suspended until the lock is released.
     **/
    void Spinlock();
    /**
     * Unlock the StickyLock after a SpinLock() call
     **/
    void Unlock();

    /**
     * Try to lock the StickyLock.
     * Interrupts will be disabled until the lock is released.
     * Should be used instead of SpinLock() when the lock might be acquired in an interrupt handler
     **/
    void Spinlock_Cli();
    /**
     * Unlock the StickyLock after a SpinLock_Cli() call
     **/
    void Unlock_Cli();

    /**
     * Try to lock the StickyLock whithout setting the sticky bit of the calling thread.
     * Should basically only be used in interrupt handlers.
     **/
    void Spinlock_Raw();
    /**
     * Unlock the StickyLock after a Spinlock_Raw() call
     **/
    void Unlock_Raw();

    bool TryLock();

private:
    void DoUnlock();

private:
    uint64 m_Val;
};