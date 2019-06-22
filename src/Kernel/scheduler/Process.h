#pragma once

#include "types.h"
#include "Mutex.h"
#include "ktl/vector.h"
#include "ktl/list.h"
#include "Thread.h"

struct ProcessFileDescriptor {
    uint64 id;
    uint64 desc;
};

struct ProcessInfo {
    uint64 pid;

    uint64 pml4Entry;

    Mutex fileDescLock;
    ktl::vector<ProcessFileDescriptor*> fileDescs;

    ktl::nlist<ThreadInfo> threads;
};