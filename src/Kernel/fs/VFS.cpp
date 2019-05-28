#include "VFS.h"

#include "stl/list.h"
#include "stdlib/string.h"
#include "memory/memutil.h"

namespace VFS {

    struct MountPoint {
        MountPoint* next;
        MountPoint* prev;

        char path[255];
        FileSystem* fs;

        Mutex childMountLock;
        std::nlist<MountPoint> childMounts;

        Mutex nodeCacheLock;
        std::nlist<Node> nodeCache;
        uint64 firstFreeNode;
    };

    static MountPoint* g_RootMount = nullptr;

    static void GetFolderPath(const char* path, char* buffer) {
        int lastSep = 0;

        int index = 0;
        while(path[index] != '\0') {
            buffer[index] = path[index];
            if(path[index] == '/')
                lastSep = index;
            index++;
        }

        buffer[lastSep] = '\0';
    }

    bool CreateFile(const char* path) {
        char folderBuffer[255];

        
    }

    bool Mount(const char* mountPoint, FileSystem* fs) {
        if(g_RootMount == nullptr && strcmp(mountPoint, "/") == 0) {
            g_RootMount = new MountPoint();
            strcpy(g_RootMount->path, mountPoint);
            g_RootMount->fs = fs;
            g_RootMount->firstFreeNode = (uint64)-1;
            return true;
        } else {

        }
    }

}