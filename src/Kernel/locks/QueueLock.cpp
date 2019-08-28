#include "QueueLock.h"

#include "scheduler/Scheduler.h"

QueueLock::QueueLock()
    : m_Count(1)
{ }
QueueLock::QueueLock(uint64 count)
    : m_Count(count)
{ }

void QueueLock::Lock() {
    m_Lock.Spinlock();
    if(m_Count > 0) {
        m_Count--;
        m_Lock.Unlock();
    } else {
        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
        tInfo->blockEvent.type = ThreadBlockEvent::TYPE_QUEUE_LOCK;

        QueueLockEntry entry;
        entry.thread = tInfo;
        m_Queue.push_back(&entry);
        m_Lock.Unlock();

        Scheduler::ThreadYield();
    }
}

void QueueLock::Unlock() {
    m_Lock.Spinlock();
    if(m_Queue.empty()) {
        m_Count++;
        m_Lock.Unlock();
    } else {
        QueueLockEntry* e = m_Queue.front();
        m_Queue.pop_front();
        e->thread->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        m_Lock.Unlock();
    }
}