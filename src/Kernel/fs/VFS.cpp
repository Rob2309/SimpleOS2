#include "VFS.h"

#include "ktl/list.h"
#include "klib/string.h"
#include "klib/memory.h"
#include "klib/stdio.h"
#include "devices/DeviceDriver.h"
#include "PipeFS.h"

namespace VFS {
    
    struct CachedNode {
        CachedNode* next;
        CachedNode* prev;

        uint64 softRefCount;

        uint64 nodeID; // ID of the contained node

        Node node;
    };

    struct MountPoint {
        MountPoint* next;
        MountPoint* prev;

        char path[255];
        FileSystem* fs;
        SuperBlock sb;

        Mutex childMountLock;
        ktl::nlist<MountPoint> childMounts;

        Mutex nodeCacheLock;
        ktl::nlist<CachedNode> nodeCache;
    };

    struct FileDescriptor {
        FileDescriptor* next;
        FileDescriptor* prev;

        uint64 refCount;

        MountPoint* mp;
        CachedNode* node;

        uint64 pos;
    };

    static MountPoint* g_RootMount = nullptr;
    static MountPoint* g_PipeMount = nullptr;

    static Mutex g_FileDescLock;
    static ktl::nlist<FileDescriptor> g_FileDescs;

    static bool CleanPath(const char* path, char* cleanBuffer) {
        int length = kstrlen(path);
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

        if(lastSep == 0) {
            path[253] = '/';
            path[254] = '\0';

            *folder = &path[253];
            *file = &path[1];
        } else {
            path[lastSep] = '\0';
            *folder = path;
            *file = &path[lastSep+1];
        }
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

    static CachedNode* AcquireNode(MountPoint* mp, uint64 nodeID) {
        mp->nodeCacheLock.SpinLock();
        for(CachedNode* n : mp->nodeCache) {
            if(n->nodeID == nodeID) {
                n->softRefCount++;
                mp->nodeCacheLock.Unlock();
                n->node.lock.SpinLock();
                return n;
            }
        }

        CachedNode* newNode = new CachedNode();
        newNode->nodeID = nodeID;
        newNode->softRefCount = 1;
        newNode->node.lock.SpinLock();
        mp->nodeCache.push_back(newNode);
        mp->nodeCacheLock.Unlock();

        mp->fs->ReadNode(nodeID, &newNode->node);
        return newNode;
    }

    static void ReleaseNode(MountPoint* mp, CachedNode* node, bool decrement = true) {
        mp->nodeCacheLock.SpinLock();

        if(decrement)
            node->softRefCount--;

        if(node->node.linkRefCount == 0 && node->softRefCount == 0) {
            mp->nodeCache.erase(node);
            mp->nodeCacheLock.Unlock();

            mp->fs->DestroyNode(&node->node);
            if(node->node.type == Node::TYPE_DIRECTORY && node->node.dir != nullptr)
                Directory::Destroy(node->node.dir);
            delete node;
        } else {
            node->node.lock.Unlock();
            mp->nodeCacheLock.Unlock();
        }
    }

    static Directory* GetDir(Node* node) {
        if(node->dir == nullptr)
            node->dir = node->fs->ReadDirEntries(node);

        return node->dir;
    }

    static bool PathWalk(const char** pathPtr, const char* entry) {
        const char* path = *pathPtr;

        while(true) {
            if(*entry == '\0') {
                if(*path == '/') {
                    path++;
                    *pathPtr = path;
                    return true;
                } else if(*path == '\0') {
                    *pathPtr = path;
                    return true;
                } else {
                    return false;
                }
            }

            if(*entry != *path)
                return false;

            entry++;
            path++;
        }
    }

    static CachedNode* AcquirePath(MountPoint* mp, const char* path) {
        CachedNode* current = AcquireNode(mp, mp->sb.rootNode);

        if(*path == '\0')
            return current;

        while(true) {
            if(current->node.type != Node::TYPE_DIRECTORY) {
                ReleaseNode(mp, current);
                return nullptr;
            }

            Directory* dir = GetDir(&current->node);
            CachedNode* next = nullptr;
            for(uint64 i = 0; i < dir->numEntries; i++) {
                if(PathWalk(&path, dir->entries[i].name)) {
                    next = AcquireNode(mp, dir->entries[i].nodeID);
                    ReleaseNode(mp, current);

                    if(*path == '\0')
                        return next;
                }
            }

            if(next == nullptr) {
                ReleaseNode(mp, current);
                return nullptr;
            } else {
                current = next;
            }
        }
    }

    static CachedNode* CreateNode(MountPoint* mp, uint64 softRefs = 1) {
        CachedNode* newNode = new CachedNode();
        mp->fs->CreateNode(&newNode->node);
        newNode->nodeID = newNode->node.id;
        newNode->softRefCount = softRefs;
        newNode->node.lock.SpinLock();

        mp->nodeCacheLock.SpinLock();
        mp->nodeCache.push_back(newNode);
        mp->nodeCacheLock.Unlock();

        return newNode;
    }

    void Init(FileSystem* rootFS) {
        MountPoint* mp = new MountPoint();
        mp->fs = rootFS;
        rootFS->GetSuperBlock(&mp->sb);
        kstrcpy(mp->path, "/");
        g_RootMount = mp;

        g_PipeMount = new MountPoint();
        g_PipeMount->fs = new PipeFS();
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

        CachedNode* folderNode = AcquirePath(mp, folderPath);
        if(folderNode == nullptr)
            return false;
        if(folderNode->node.type != Node::TYPE_DIRECTORY) {
            ReleaseNode(mp, folderNode);
            return false;
        }

        GetDir(&folderNode->node);
        DirectoryEntry* newEntry;
        Directory::AddEntry(&folderNode->node.dir, &newEntry);

        CachedNode* newNode = CreateNode(mp);
        newNode->node.linkRefCount = 1;
        newNode->node.type = Node::TYPE_FILE;
        
        newEntry->nodeID = newNode->nodeID;
        kstrcpy(newEntry->name, fileName);

        ReleaseNode(mp, folderNode);
        ReleaseNode(mp, newNode);

        return true;
    }

    bool CreateFolder(const char* path) {
        char cleanBuffer[255];
        if(!CleanPath(path, cleanBuffer))
            return false;

        const char* folderPath;
        const char* fileName;
        SplitFolderAndFile(cleanBuffer, &folderPath, &fileName);

        MountPoint* mp = AcquireMountPoint(folderPath);
        folderPath = AdvancePath(folderPath, mp->path);

        CachedNode* folderNode = AcquirePath(mp, folderPath);
        if(folderNode == nullptr)
            return false;
        if(folderNode->node.type != Node::TYPE_DIRECTORY) {
            ReleaseNode(mp, folderNode);
            return false;
        }

        GetDir(&folderNode->node);
        DirectoryEntry* newEntry;
        Directory::AddEntry(&folderNode->node.dir, &newEntry);

        CachedNode* newNode = CreateNode(mp);
        newNode->node.linkRefCount = 1;
        newNode->node.type = Node::TYPE_DIRECTORY;
        newNode->node.dir = Directory::Create(10);
        
        newEntry->nodeID = newNode->nodeID;
        kstrcpy(newEntry->name, fileName);

        ReleaseNode(mp, folderNode);
        ReleaseNode(mp, newNode);

        return true;
    }

    bool CreateDeviceFile(const char* path, uint64 driverID, uint64 subID) {
        char cleanBuffer[255];
        if(!CleanPath(path, cleanBuffer))
            return false;

        const char* folderPath;
        const char* fileName;
        SplitFolderAndFile(cleanBuffer, &folderPath, &fileName);

        MountPoint* mp = AcquireMountPoint(folderPath);
        folderPath = AdvancePath(folderPath, mp->path);

        CachedNode* folderNode = AcquirePath(mp, folderPath);
        if(folderNode == nullptr)
            return false;
        if(folderNode->node.type != Node::TYPE_DIRECTORY) {
            ReleaseNode(mp, folderNode);
            return false;
        }

        GetDir(&folderNode->node);
        DirectoryEntry* newEntry;
        Directory::AddEntry(&folderNode->node.dir, &newEntry);

        DeviceDriver* driver = DeviceDriverRegistry::GetDriver(driverID);

        CachedNode* newNode = CreateNode(mp);
        newNode->node.linkRefCount = 1;
        newNode->node.type = driver->GetType() == DeviceDriver::TYPE_CHAR ? Node::TYPE_DEVICE_CHAR : Node::TYPE_DEVICE_BLOCK;
        newNode->node.device.driverID = driverID;
        newNode->node.device.subID = subID;
        
        newEntry->nodeID = newNode->nodeID;
        kstrcpy(newEntry->name, fileName);

        ReleaseNode(mp, folderNode);
        ReleaseNode(mp, newNode);

        return true;
    }
    bool CreatePipe(uint64* readDesc, uint64* writeDesc) {
        CachedNode* pipeNode = CreateNode(g_PipeMount, 2);
        pipeNode->node.type = Node::TYPE_PIPE;
        ReleaseNode(g_PipeMount, pipeNode, false);

        FileDescriptor* descRead = new FileDescriptor();
        descRead->mp = g_PipeMount;
        descRead->node = pipeNode;
        descRead->pos = 0;
        descRead->refCount = 1;

        FileDescriptor* descWrite = new FileDescriptor();
        descWrite->mp = g_PipeMount;
        descWrite->node = pipeNode;
        descWrite->pos = 0;
        descWrite->refCount = 1;

        *readDesc = (uint64)descRead;
        *writeDesc = (uint64)descWrite;
    }

    bool Delete(const char* path) {
        char cleanBuffer[255];
        if(!CleanPath(path, cleanBuffer))
            return false;

        MountPoint* mp = AcquireMountPoint(cleanBuffer);
        char* relPath = (char*)AdvancePath(cleanBuffer, mp->path);
        if(*relPath == '\0') // trying to delete a mountpoint
            return false;
        
        const char* folderPath;
        const char* fileName;
        SplitFolderAndFile(relPath, &folderPath, &fileName);

        CachedNode* folderNode = AcquirePath(mp, folderPath);
        if(folderNode == nullptr)
            return false;
        if(folderNode->node.type != Node::TYPE_DIRECTORY) {
            ReleaseNode(mp, folderNode);
            return false;
        }

        Directory* dir = GetDir(&folderNode->node);
        for(uint64 i = 0; i < dir->numEntries; i++) {
            if(kstrcmp(fileName, dir->entries[i].name) == 0) {
                uint64 nodeID = dir->entries[i].nodeID;
                CachedNode* fileNode = AcquireNode(mp, nodeID);

                if(fileNode->node.type == Node::TYPE_DIRECTORY) {
                    Directory* dir = GetDir(&fileNode->node);
                    if(dir->numEntries != 0) {
                        ReleaseNode(mp, fileNode);
                        ReleaseNode(mp, folderNode);
                        return false;
                    }
                }

                Directory::RemoveEntry(&folderNode->node.dir, i);
                fileNode->node.linkRefCount--;

                ReleaseNode(mp, fileNode);
                ReleaseNode(mp, folderNode);
                return true;
            }
        }

        ReleaseNode(mp, folderNode);
        return false;
    }

    bool Mount(const char* mountPoint, FileSystem* fs) {
        char cleanBuffer[255];
        if(!CleanPath(mountPoint, cleanBuffer))
            return false;

        MountPoint* mp = AcquireMountPoint(cleanBuffer);
        const char* folderPath = AdvancePath(cleanBuffer, mp->path);
        if(*folderPath == '\0') // Trying to mount onto mountpoint
            return false;

        CachedNode* folderNode = AcquirePath(mp, folderPath);
        if(folderNode == nullptr)
            return false;
        if(folderNode->node.type != Node::TYPE_DIRECTORY) {
            ReleaseNode(mp, folderNode);
            return false;
        }
        
        Directory* dir = GetDir(&folderNode->node);
        if(dir->numEntries != 0) {
            ReleaseNode(mp, folderNode);
            return false;
        }

        MountPoint* newMP = new MountPoint();
        kstrcpy(newMP->path, cleanBuffer);
        newMP->fs = fs;
        fs->GetSuperBlock(&newMP->sb);
        
        mp->childMountLock.SpinLock();
        mp->childMounts.push_back(newMP);
        mp->childMountLock.Unlock();
    }

    bool Unmount(const char* mountPoint) {
        return false;
    }

    uint64 Open(const char* path) {
        char cleanBuffer[255];
        if(!CleanPath(path, cleanBuffer))
            return false;

        MountPoint* mp = AcquireMountPoint(cleanBuffer);
        path = AdvancePath(cleanBuffer, mp->path);

        CachedNode* node = AcquirePath(mp, path);
        if(node == nullptr)
            return 0;
        if(node->node.type == Node::TYPE_DIRECTORY) {
            ReleaseNode(mp, node);
            return 0;
        }
        
        ReleaseNode(mp, node, false);

        FileDescriptor* desc = new FileDescriptor();
        desc->mp = mp;
        desc->node = node;
        desc->pos = 0;
        desc->refCount = 1;

        return (uint64)desc;
    }

    void Close(uint64 descID) {
        FileDescriptor* desc = (FileDescriptor*)descID;

        desc->refCount--;
        if(desc->refCount == 0) {
            CachedNode* node = desc->node;
            node->node.lock.SpinLock();
            ReleaseNode(desc->mp, node);

            delete desc;
        }
    }

    void AddRef(uint64 descID) {
        FileDescriptor* desc = (FileDescriptor*)descID;

        desc->refCount++;
    }

    bool List(const char* path, FileList* list, bool getTypes) {
        char cleanBuffer[255];
        if(!CleanPath(path, cleanBuffer))
            return false;

        MountPoint* mp = AcquireMountPoint(cleanBuffer);
        path = AdvancePath(cleanBuffer, mp->path);

        CachedNode* node = AcquirePath(mp, path);
        if(node == nullptr)
            return false;
        if(node->node.type != Node::TYPE_DIRECTORY) {
            ReleaseNode(mp, node);
            return false;
        }
        
        Directory* dir = GetDir(&node->node);
        list->numEntries = dir->numEntries;
        list->entries = new FileList::Entry[list->numEntries];
        for(uint64 i = 0; i < list->numEntries; i++) {
            kstrcpy(list->entries[i].name, dir->entries[i].name);
            if(getTypes) {
                CachedNode* entryNode = AcquireNode(mp, dir->entries[i].nodeID);
                list->entries[i].type = entryNode->node.type;
                ReleaseNode(mp, entryNode);
            }
        }

        ReleaseNode(mp, node);
        return true;
    }

    static uint64 _Read(Node* node, uint64 pos, void* buffer, uint64 bufferSize) {
        uint64 res;
        if(node->type == Node::TYPE_DEVICE_CHAR) {
            CharDeviceDriver* driver = (CharDeviceDriver*)DeviceDriverRegistry::GetDriver(node->device.driverID);
            if(driver == nullptr)
                return 0;
            res = driver->Read(node->device.subID, buffer, bufferSize);
        } else if(node->type == Node::TYPE_DEVICE_BLOCK) {
            BlockDeviceDriver* driver = (BlockDeviceDriver*)DeviceDriverRegistry::GetDriver(node->device.driverID);
            if(driver == nullptr)
                return 0;
            
            uint64 devID = node->device.subID;
            uint64 blockSize = driver->GetBlockSize(devID);
            uint64 startBlock = pos / blockSize;
            uint64 endBlock = (pos + bufferSize) / blockSize;
            uint64 numBlocks = endBlock - startBlock + 1;
            uint64 offset = pos - startBlock * blockSize;

            char* tempBuffer = new char[numBlocks * blockSize];
            Atomic<uint64> res;
            res.Write(0);
            driver->ScheduleOperation(devID, startBlock, numBlocks, false, tempBuffer, &res);
            while(res.Read() != 1) ;

            kmemcpy(buffer, tempBuffer + offset, bufferSize);
            delete[] tempBuffer;

            return bufferSize;
        } else {
            res = node->fs->ReadNodeData(node, pos, buffer, bufferSize);
        }
        return res;
    }

    uint64 Read(uint64 descID, void* buffer, uint64 bufferSize) {
        FileDescriptor* desc = (FileDescriptor*)descID;
        
        uint64 res = _Read(&desc->node->node, desc->pos, buffer, bufferSize);
        desc->pos += res;

        return res;
    }

    uint64 Read(uint64 descID, uint64 pos, void* buffer, uint64 bufferSize) {
        FileDescriptor* desc = (FileDescriptor*)descID;

        return _Read(&desc->node->node, pos, buffer, bufferSize);
    }

    static uint64 _Write(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) {
        uint64 res;
        if(node->type == Node::TYPE_DEVICE_CHAR) {
            CharDeviceDriver* driver = (CharDeviceDriver*)DeviceDriverRegistry::GetDriver(node->device.driverID);
            if(driver == nullptr)
                return 0;
            res = driver->Write(node->device.subID, buffer, bufferSize);
        } else if(node->type == Node::TYPE_DEVICE_BLOCK) {
            return 0;
        } else {
            res = node->fs->WriteNodeData(node, pos, buffer, bufferSize);
        }
        return res;
    }

    uint64 Write(uint64 descID, const void* buffer, uint64 bufferSize) {
        FileDescriptor* desc = (FileDescriptor*)descID;
        
        uint64 res = _Write(&desc->node->node, desc->pos, buffer, bufferSize);
        desc->pos += res;

        return res;
    }

    uint64 Write(uint64 descID, uint64 pos, const void* buffer, uint64 bufferSize) {
        FileDescriptor* desc = (FileDescriptor*)descID;

        return _Write(&desc->node->node, pos, buffer, bufferSize);
    }

    void Stat(uint64 descID, NodeStats* stats) {
        FileDescriptor* desc = (FileDescriptor*)descID;

        stats->size = desc->node->node.fileSize;
    }

}