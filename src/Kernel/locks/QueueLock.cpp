#include "QueueLock.h"

#include "scheduler/Scheduler.h"

void QueueLock::Lock() {
    m_Lock.SpinLock();
    if(!m_Locked) {
        m_Locked = true;
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
    m_Lock.SpinLock();
    if(m_Queue.empty()) {
        m_Locked = false;
        m_Lock.Unlock();
    } else {
        QueueLockEntry* e = m_Queue.front();
        m_Queue.pop_front();
        e->thread->blockEvent.type = ThreadBlockEvent::TYPE_NONE;
        m_Lock.Unlock();
    }
}

void QueueLock::SetLocked() {
    m_Locked = true;
}