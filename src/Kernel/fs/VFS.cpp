#include "VFS.h"

#include "stl/list.h"
#include "stdlib/string.h"

namespace VFS {

    constexpr uint64 NodeBlockLength = 32;

    struct NodeBlock {
        NodeBlock* next;
        NodeBlock* prev;

        uint64 refCount;

        uint64 baseNode;
        Node nodes[NodeBlockLength];
        Directory* directories[NodeBlockLength];
    };

    struct SuperBlock {
        uint64 rootNode;
    };

    struct MountPoint {
        MountPoint* next;
        MountPoint* prev;

        FileSystem* fs;
        SuperBlock sb;
        char path[255];

        std::nlist<NodeBlock> nodeCache;
    };

    static Mutex g_MountPointsLock;
    static std::nlist<MountPoint> g_MountPoints;

    bool Mount(const char* mountPoint, FileSystem* fs) {
        g_MountPointsLock.SpinLock();
        if(strcmp(mountPoint, "/") == 0 && g_MountPoints.empty()) {
            MountPoint* mp = new MountPoint();
            mp->fs = fs;
            strcpy(mp->path, "/");
            fs->GetSuperBlock(&mp->sb);
            g_MountPoints.push_back(mp);
            g_MountPointsLock.Unlock();
            return true;
        }



        g_MountPointsLock.Unlock();
    }

    static bool MountCmp(const char* mount, const char* path) {
        int i = 0;
        while(true) {
            if(mount[i] == '\0') {
                return path[i] == '\0' || path[i] == '/';
            }

            if(mount[i] != path[i])
                return false;

            i++;
        }
    }

    static const char* AdvancePath(const char* mount, const char* path) {
        while(*mount != '\0') {
            mount++;
            path++;
        }

        if(*path == '/')
            path++;

        return path;
    }

    static MountPoint* FindMountPoint(const char* path) {
        g_MountPointsLock.SpinLock();
        for(MountPoint* mp : g_MountPoints) {
            if(MountCmp(mp->path, path)) {
                g_MountPointsLock.Unlock();
                return mp;
            }
        }
        g_MountPointsLock.Unlock();
        return nullptr;
    }

    static Node* AcquireNode(FileSystem* fs, uint64 id) {
        
    }

    bool CreateFile(const char* path) {
        MountPoint* mp = FindMountPoint(path);
        if(mp == nullptr)
            return false;

        path = AdvancePath(mp->path, path);


    }

}