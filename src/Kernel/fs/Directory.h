#pragma once

#include "types.h"

namespace VFS {

    struct DirectoryEntry {
        char name[50];
        uint64 nodeID;
    };
    class Directory {
    public:
        uint64 numEntries;
        uint64 capacity;
        DirectoryEntry entries[];

    public:
        DirectoryEntry* FindEntry(const char** pathPtr, uint64* entryIndex);

    public:
        static Directory* Create(uint64 capacity);
        static void Destroy(Directory* dir);
        static void AddEntry(Directory** dir, DirectoryEntry** outEntry);
        static void RemoveEntry(Directory** dir, uint64 index);
    };
    
}
