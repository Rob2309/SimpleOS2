#include "StickyLock.h"

#include "scheduler/Scheduler.h"

StickyLock::StickyLock()
    : m_Val(0)
{ }

void StickyLock::SpinLock() {
    Scheduler::ThreadSetSticky();

    while(!TryLock()) ;
}
void StickyLock::Unlock() {
    DoUnlock();

    Scheduler::ThreadUnsetSticky();
}

void StickyLock::SpinLock_NoSticky() {
    while(!TryLock()) ;
}
void StickyLock::Unlock_NoSticky() {
    DoUnlock();
}

bool StickyLock::TryLock() {
    uint64 val = 1;
    uint64* loc = &m_Val;
    __asm__ __volatile__ (
        "lock xchgq %0, (%1)"
        : "+r"(val), "+r"(loc)
    );
    return val == 0;
}
void StickyLock::DoUnlock() {
    uint64 val = 0;
    uint64* loc = &m_Val;
    __asm__ __volatile__ (
        "lock xchgq %0, (%1)"
        : "+r"(val), "+r"(loc)
    );
}