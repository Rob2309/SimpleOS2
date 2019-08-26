#pragma once

#include "types.h"
#include "locks/StickyLock.h"
#include "ktl/vector.h"
#include "ktl/list.h"
#include "Thread.h"
#include "user/User.h"

struct ProcessFileDescriptor {
    int64 id;
    uint64 desc;
};

struct ProcessInfo {
    uint64 pid;

    uint64 pml4Entry;

    StickyLock fileDescLock;
    ktl::vector<ProcessFileDescriptor> fileDescs;

    StickyLock mainLock;
    uint64 numThreads;

    User* owner;
};