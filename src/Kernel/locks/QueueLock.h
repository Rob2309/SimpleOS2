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
    QueueLock();
    QueueLock(uint64 init);

    void Lock();
    void Unlock();

private:
    StickyLock m_Lock;
    uint64 m_Count;
    ktl::nlist<QueueLockEntry> m_Queue;
};