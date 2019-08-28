#pragma once

#include "types.h"
#include "Permissions.h"
#include "locks/StickyLock.h"
#include "locks/QueueLock.h"
#include "atomic/Atomics.h"

namespace VFS {

    struct MountPoint;
    struct Directory;

    struct Node
    {
        Node* next;
        Node* prev;

        uint64 id;
        MountPoint* mp;

        uint64 refCount;
        Atomic<uint64> linkCount;    // How often this node is referenced by directory entries

        bool ready;
        QueueLock readyQueue;

        enum Type {
            TYPE_FILE,              // Normal File
            TYPE_DIRECTORY,         // Directory, containing other nodes
            TYPE_DEVICE_CHAR,       // Character Device File
            TYPE_DEVICE_BLOCK,      // Block Device File
            TYPE_PIPE,              // (Named) Pipe
            TYPE_SYMLINK,           // Symbolic link
        } type;

        StickyLock dirLock;

        union {
            struct {
                Directory* dir;
            } infoFolder;
            struct {
                Atomic<uint64> fileSize;
            } infoFile;
            struct {
                uint64 driverID;
                uint64 subID;
            } infoDevice;
            struct {
                char* linkPath;
            } infoSymlink;
        };

        uint64 ownerUID;
        uint64 ownerGID;
        Permissions permissions;

        void* fsData;
    };

}