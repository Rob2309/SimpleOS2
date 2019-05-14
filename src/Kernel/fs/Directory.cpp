#include "Directory.h"

namespace VFS {

    static bool PathWalk(const char** pathPtr, const char* nameCmp) {
        const char* path = *pathPtr;

        int index = 0;
        while(true) {
            if(nameCmp[index] == '\0') {
                if(path[index] == '/') {
                    *pathPtr += index + 1;
                    return true;
                } else if(path[index] == '\0') {
                    *pathPtr += index;
                    return true;
                } else {
                    return false;
                }
            }

            if(path[index] != nameCmp[index])
                return false;

            index++;
        }
    }

    DirectoryEntry* Directory::FindEntry(const char** pathPtr, uint64* entryIndex) {
        for(uint64 i = 0; i < numEntries; i++) {
            if(PathWalk(pathPtr, entries[i].name)) {
                if(entryIndex != nullptr)
                    *entryIndex = i;
                return &entries[i];
            }
        }

        return nullptr;
    }

    Directory* Directory::Create(uint64 capacity) {
        Directory* dir = (Directory*)new char[sizeof(Directory) + capacity * sizeof(DirectoryEntry)];
        dir->capacity = capacity;
        dir->numEntries = 0;
        return dir;
    }
    void Directory::Destroy(Directory* dir) {
        delete[] (char*)dir;
    }
    void Directory::AddEntry(Directory** dirPtr, DirectoryEntry** outEntry) {
        Directory* dir = *dirPtr;

        if(dir->capacity > dir->numEntries) {
            *outEntry = &dir->entries[dir->numEntries];
            dir->numEntries++;
            return;
        } else {
            Directory* newDir = Directory::Create(dir->capacity + 16);
            newDir->numEntries = dir->numEntries + 1;
            for(uint64 i = 0; i < dir->numEntries; i++) {
                newDir->entries[i] = dir->entries[i];
            }
            Directory::Destroy(dir);
            *dirPtr = newDir;
            *outEntry = &newDir->entries[newDir->numEntries - 1];
        }
    }
    void Directory::RemoveEntry(Directory** dirPtr, uint64 index) {
        Directory* dir = *dirPtr;

        for(uint64 i = index + 1; i < dir->numEntries; i++) {
            dir->entries[i-1] = dir->entries[i];
        }

        dir->numEntries--;
    }

}