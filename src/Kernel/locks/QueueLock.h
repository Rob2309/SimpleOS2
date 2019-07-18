#pragma once

#include "types.h"
#include "StickyLock.h"
#include "ktl/list.h"

#include "scheduler/Thread.h"

struct QueueLockEntry {
    QueueLockEntry* next;
    QueueLockEntry* prev;

    ThreadInfo* thread;
};

/**
 * This type of lock should only be used for fairly long critical sections.
 * If the lock cannot be acquired, the thread sleeps until another thread unlocks it.
 */
class QueueLock {
public:
    void Lock();
    void Unlock();

    /**
     * Forces the QueueLock to the locked state, without checking if it is available
     **/
    void SetLocked();

private:
    StickyLock m_Lock;
    bool m_Locked;
    ktl::nlist<QueueLockEntry> m_Queue;
};