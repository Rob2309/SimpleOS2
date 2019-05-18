#include "VFS.h"

#include "stl/list.h"
#include "stdlib/string.h"

namespace VFS {

    constexpr uint64 NodeBlockLength = 32;

    struct NodeBlock {
        NodeBlock* next;
        NodeBlock* prev;

        uint64 baseNode;

        Mutex nodesLock;
        Node nodes[NodeBlockLength];
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

        Mutex nodeCacheLock;
        std::nlist<NodeBlock> nodeCache;

        Mutex childMountLock;
        std::nlist<MountPoint> childMounts;
    };

    static MountPoint* g_RootMount = nullptr;

    bool Mount(const char* mountPoint, FileSystem* fs) {
        if(strcmp(mountPoint, "/") == 0 && g_RootMount == nullptr) {
            MountPoint* mp = new MountPoint();
            mp->fs = fs;
            strcpy(mp->path, "/");
            fs->GetSuperBlock(&mp->sb);
            return true;
        }
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

    static MountPoint* AcquireMountPoint(const char* path) {
        MountPoint* current = g_RootMount;
        current->childMountLock.SpinLock();

        while(true) {
            MountPoint* next = nullptr;

            for(MountPoint* mp : current->childMounts) {
                if(MountCmp(mp->path, path)) {
                    mp->childMountLock.SpinLock();
                    current->childMountLock.Unlock();
                    next = mp;
                    break;
                }
            }

            if(next != nullptr) {
                current = next;
            } else {
                current->childMountLock.Unlock();
                return current;
            }
        }
    }

    static bool PathWalk(const char** pathPtr, const char* name) {
        const char* path = *pathPtr;

        while(true) {
            if(*name == '\0') {
                if(*path == '/') {
                    *pathPtr = path + 1;
                    return true;
                } else if(*path == '\0') {
                    *pathPtr = path;
                    return true;
                } else {
                    return false;
                }
            }

            if(*name != *path)
                return false;

            name++;
            path++;
        }
    }

    static NodeBlock* AcquireNodeBlock(MountPoint* mp, uint64 nodeID) {
        mp->nodeCacheLock.SpinLock();
        for(NodeBlock* nb : mp->nodeCache) {
            if(nb->baseNode <= nodeID && nb->baseNode + NodeBlockLength > nodeID) {
                mp->nodeCacheLock.Unlock();
                return nb;
            }
        }
        
        NodeBlock* nb = new NodeBlock();
        nb->baseNode = nodeID / NodeBlockLength * NodeBlockLength;
        mp->nodeCache.push_back(nb);
        mp->nodeCacheLock.Unlock();
        return nb;
    }
    static Node* AcquireNode(MountPoint* mp, uint64 nodeID) {
        NodeBlock* nb = AcquireNodeBlock(mp, nodeID);

        uint64 index = nodeID - nb->baseNode;
        Node* node = &nb->nodes[index];
        node->lock.SpinLock();

        if(node->id == 0) {
            mp->fs->ReadNode(nodeID, node);
        }

        return node;
    }
    static Node* CreateNode(MountPoint* mp) {
        uint64 nodeID = mp->fs->GetFreeNode();
        Node* node = AcquireNode(mp, nodeID);
        return node;
    }
    static void ReleaseNode(Node* node) {
        if(node->refCount == 0) {
            node->fs->DestroyNode(node);
            if(node->type == Node::TYPE_DIRECTORY && node->dir != nullptr) {
                Directory::Destroy(node->dir);
                node->dir = nullptr;
            }
        }
        node->lock.Unlock();
    }

    static Directory* GetDirectory(Node* node) {
        if(node->dir == nullptr) {
            node->dir = node->fs->ReadDirEntries(node);
        }
        return node->dir;
    }

    static void RemoveFileName(const char* path, char* buffer) {
        uint64 lastSlashPos = 0;

        uint64 i = 0;
        while(true) {
            buffer[i] = path[i];
            if(path[i] == '/')
                lastSlashPos = i;
            if(path[i] == '\0')
                break;

            i++;
        }

        buffer[lastSlashPos] = '\0';
    }

    Node* AcquirePath(MountPoint* mp, const char* path) {
        Node* node = AcquireNode(mp, mp->sb.rootNode);
        if(path[0] == '\0') 
            return node;

        while(true) {
            if(node->type != Node::TYPE_DIRECTORY) {
                node->lock.Unlock();
                return nullptr;
            }

            Directory* dir = GetDirectory(node);
            Node* next = nullptr;

            for(uint64 i = 0; i < dir->numEntries; i++) {
                if(PathWalk(&path, dir->entries[i].name)) {
                    next = AcquireNode(mp, dir->entries[i].nodeID);
                    node->lock.Unlock();
                    
                    if(path[0] == '\0') {
                        return next;
                    }

                    break;
                }
            }

            if(next == nullptr) {
                node->lock.Unlock();
                return nullptr;
            }

            node = next;
        }
    }

    bool CreateFile(const char* path) {
        char folderBuffer[255];
        RemoveFileName(path, folderBuffer);

        MountPoint* mp = AcquireMountPoint(folderBuffer);
        const char* folderPath = AdvancePath(mp->path, folderBuffer);
        const char* fileName = AdvancePath(folderBuffer, path);

        Node* folder = AcquirePath(mp, folderPath);
        if(folder == nullptr)
            return false;
        if(folder->type != Node::TYPE_DIRECTORY) {
            ReleaseNode(folder);
            return false;
        }

        Node* file = CreateNode(mp);
        file->type = Node::TYPE_FILE;
        file->refCount = 1;
        
        GetDirectory(folder);
        DirectoryEntry* entry;
        Directory::AddEntry(&folder->dir, &entry);
        strcpy(entry->name, fileName);
        entry->nodeID = file->id;

        ReleaseNode(file);
        ReleaseNode(folder);
        return true;
    }

}