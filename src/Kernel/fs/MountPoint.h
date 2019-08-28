#pragma once

#include "types.h"
#include "atomic/Atomics.h"
#include "Node.h"
#include "SuperBlock.h"

namespace VFS {

    struct FileSystem;

    struct MountPoint {
        MountPoint* next;
        MountPoint* prev;
        MountPoint* parent;

        char path[255];
        FileSystem* fs;
        SuperBlock sb;

        StickyLock childMountLock;
        ktl::nlist<MountPoint> childMounts;

        StickyLock nodeCacheLock;
        ktl::nlist<Node> nodeCache;

        Atomic<uint64> refCount;
    };

}