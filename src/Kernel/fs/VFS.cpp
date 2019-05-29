#include "VFS.h"

#include "stl/list.h"
#include "stdlib/string.h"
#include "memory/memutil.h"
#include "terminal/conio.h"

namespace VFS {

    struct MountPoint {
        MountPoint* next;
        MountPoint* prev;

        char path[255];
        FileSystem* fs;
        SuperBlock sb;

        Mutex childMountLock;
        std::nlist<MountPoint> childMounts;

        Mutex nodeCacheLock;
        std::nlist<Node> nodeCache;
        uint64 firstFreeNode;
    };

    static MountPoint* g_RootMount = nullptr;

    static bool CleanPath(const char* path, char* cleanBuffer) {
        int length = strlen(path);
        if(length >= 255)
            return false;
        
        if(path[0] != '/')
            return false;
        cleanBuffer[0] = '/';

        int bufferPos = 1;
        for(int i = 1; i < length; i++) {
            char c = path[i];
            if(c == '/' && cleanBuffer[bufferPos - 1] == '/') {
            } else {
                cleanBuffer[bufferPos] = c;
                bufferPos++;
            }
        }

        if(cleanBuffer[bufferPos-1] == '/')
            cleanBuffer[bufferPos - 1] = '\0';
        else
            cleanBuffer[bufferPos] = '\0';

        return true;
    }

    static void SplitFolderAndFile(char* path, const char** folder, const char** file) {
        int lastSep = 0;

        int index = 0;
        while(path[index] != '\0') {
            if(path[index] == '/')
                lastSep = index;
            index++;
        }

        path[lastSep] = '\0';

        *folder = path;
        *file = &path[lastSep+1];
    }

    static bool MountCmp(const char* path, const char* mount) {
        while(true) {
            if(*mount == '\0')
                return *path == '\0' || *path == '/';

            if(*mount != *path)
                return false;

            mount++;
            path++;
        }
    }

    static MountPoint* AcquireMountPoint(const char* path) {
        MountPoint* current = g_RootMount;
        current->childMountLock.SpinLock();

        while(true) {
            MountPoint* next = nullptr;
            for(MountPoint* mp : current->childMounts) {
                if(MountCmp(path, mp->path)) {
                    next = mp;
                    break;
                }
            }

            if(next != nullptr) {
                current->childMountLock.Unlock();
                next->childMountLock.SpinLock();
                current = next;
            } else {
                current->childMountLock.Unlock();
                return current;
            }
        }
    }

    static const char* AdvancePath(const char* path, const char* mp) {
        while(*mp != '\0') {
            path++;
            mp++;
        }

        if(*path == '/')
            path++;

        return path;
    }

    Node* AcquireNode(MountPoint* mp, uint64 nodeID) {
        mp->nodeCacheLock.SpinLock();
        for(Node* n : mp->nodeCache) {
            if(n->id.Read() == nodeID) {
                n->lock.SpinLock();
                mp->nodeCacheLock.Unlock();
                return n;
            }
        }

        Node* newCache = new Node();
        newCache->id = nodeID;
        newCache->lock.SpinLock();
        mp->nodeCache.push_back(newCache);
        mp->nodeCacheLock.Unlock();

        mp->fs->ReadNode(nodeID, newCache);
        return newCache;
    }

    Node* AcquirePath(MountPoint* mp, const char* path) {
        Node* current = AcquireNode(mp, mp->sb.rootNode);
    }

    bool CreateFile(const char* path) {
        char cleanBuffer[255];
        if(!CleanPath(path, cleanBuffer))
            return false;

        const char* folderPath;
        const char* fileName;
        SplitFolderAndFile(cleanBuffer, &folderPath, &fileName);

        MountPoint* mp = AcquireMountPoint(folderPath);
        folderPath = AdvancePath(folderPath, mp->path);

        Node* node = AcquirePath(mp, folderPath);
    }

    bool Mount(const char* mountPoint, FileSystem* fs) {
        if(g_RootMount == nullptr && strcmp(mountPoint, "/") == 0) {
            g_RootMount = new MountPoint();
            strcpy(g_RootMount->path, mountPoint);
            g_RootMount->fs = fs;
            fs->GetSuperBlock(&g_RootMount->sb);
            g_RootMount->firstFreeNode = (uint64)-1;
            return true;
        } else {
            return false;
        }
    }

}