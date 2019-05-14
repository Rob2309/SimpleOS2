#include "VFS.h"

#include "stl/list.h"
#include "stdlib/string.h"

namespace VFS {

    struct SuperBlock {
        uint64 rootNode;
    };

    struct MountPoint {
        MountPoint* next;
        MountPoint* prev;

        FileSystem* fs;
        SuperBlock sb;
        char path[255];
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

}