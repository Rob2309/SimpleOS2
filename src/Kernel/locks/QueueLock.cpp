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
        Scheduler::ThreadSetSticky();

        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
        tInfo->state.type = ThreadState::STATE_QUEUE_LOCK;

        QueueLockEntry entry;
        entry.thread = tInfo;
        m_Queue.push_back(&entry);
        m_Lock.Unlock();

        if(Scheduler::_Block()) {
            if(tInfo->killPending)
                Scheduler::ThreadExit(1);
        }

        Scheduler::ThreadUnsetSticky();
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
        e->thread->state.type = ThreadState::STATE_READY;
        e->thread->registers.rax = 0;
        m_Lock.Unlock();
    }
}