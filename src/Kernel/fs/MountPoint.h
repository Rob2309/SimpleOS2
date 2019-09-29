#pragma once

#include "types.h"
#include "atomic/Atomics.h"
#include "Node.h"
#include "SuperBlock.h"
#include "ktl/AnchorList.h"

namespace VFS {

    struct FileSystem;

    struct MountPoint {
        ktl::Anchor<MountPoint> anchor;

        MountPoint* parent;

        char path[255];
        FileSystem* fs;
        SuperBlock sb;

        StickyLock childMountLock;
        ktl::AnchorList<MountPoint, &MountPoint::anchor> childMounts;

        StickyLock nodeCacheLock;
        ktl::AnchorList<Node, &Node::anchor> nodeCache;

        Atomic<uint64> refCount;
    };

}