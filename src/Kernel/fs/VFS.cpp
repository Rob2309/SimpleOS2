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

        StickyLock childMountLock;
        ktl::nlist<MountPoint> childMounts;

        StickyLock nodeCacheLock;
        ktl::nlist<CachedNode> nodeCache;
    };

    struct FileDescriptor {
        FileDescriptor* next;
        FileDescriptor* prev;

        uint64 refCount;

        MountPoint* mp;
        CachedNode* node;

        uint8 permissions;

        uint64 pos;
    };

    static MountPoint* g_RootMount = nullptr;
    static MountPoint* g_PipeMount = nullptr;

    static StickyLock g_FileDescLock;
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
        current->childMountLock.Spinlock();

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
                next->childMountLock.Spinlock();
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
        mp->nodeCacheLock.Spinlock();
        for(CachedNode* n : mp->nodeCache) {
            if(n->nodeID == nodeID) {
                n->softRefCount++;
                mp->nodeCacheLock.Unlock();
                n->node.lock.Lock();
                return n;
            }
        }

        CachedNode* newNode = new CachedNode();
        newNode->nodeID = nodeID;
        newNode->softRefCount = 1;
        newNode->node.lock.SetLocked();
        mp->nodeCache.push_back(newNode);
        mp->nodeCacheLock.Unlock();

        mp->fs->ReadNode(nodeID, &newNode->node);
        return newNode;
    }

    static void ReleaseNode(MountPoint* mp, CachedNode* node, bool decrement = true) {
        mp->nodeCacheLock.Spinlock();

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
            mp->nodeCacheLock.Unlock();
            node->node.lock.Unlock();
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

    static bool CheckPermissions(User* user, Node* node, uint8 reqPerms) {
        if(user->uid == 0)
            return true;
            
        if(user->uid == node->ownerUID) {
            return (node->permissions.ownerPermissions & reqPerms) == reqPerms;
        } else if(user->gid == node->ownerGID) {
            return (node->permissions.groupPermissions & reqPerms) == reqPerms;
        } else {
            return (node->permissions.otherPermissions & reqPerms) == reqPerms;
        }
    }

    static int64 AcquirePath(User* user, MountPoint* mp, const char* path, CachedNode** outNode) {
        CachedNode* current = AcquireNode(mp, mp->sb.rootNode);

        if(*path == '\0') {
            *outNode = current;
            return OK;
        }

        while(true) {
            if(current->node.type != Node::TYPE_DIRECTORY) {
                ReleaseNode(mp, current);
                return ErrorFileNotFound;
            }
            if(!CheckPermissions(user, &current->node, Permissions::Read)) {
                ReleaseNode(mp, current);
                return ErrorPermissionDenied;
            }

            Directory* dir = GetDir(&current->node);
            CachedNode* next = nullptr;
            for(uint64 i = 0; i < dir->numEntries; i++) {
                if(PathWalk(&path, dir->entries[i].name)) {
                    next = AcquireNode(mp, dir->entries[i].nodeID);
                    ReleaseNode(mp, current);

                    if(*path == '\0') {
                        *outNode = next;
                        return OK;
                    }
                }
            }

            if(next == nullptr) {
                ReleaseNode(mp, current);
                return ErrorFileNotFound;
            } else {
                current = next;
            }
        }
    }

    static int64 AcquirePathAndParent(User* user, MountPoint* mp, const char* folder, const char* file, CachedNode** outParent, CachedNode** outFile) {
        CachedNode* parent;
        int64 error = AcquirePath(user, mp, folder, &parent);
        if(error != OK)
            return error;

        if(parent->node.type != Node::TYPE_DIRECTORY) {
            ReleaseNode(mp, parent);
            return ErrorFileNotFound;
        }
        if(!CheckPermissions(user, &parent->node, Permissions::Read)) {
            ReleaseNode(mp, parent);
            return ErrorPermissionDenied;
        }
        *outParent = parent;

        Directory* dir = GetDir(&parent->node);
        for(uint64 i = 0; i < dir->numEntries; i++) {
            if(PathWalk(&file, dir->entries[i].name)) {
                *outFile = AcquireNode(mp, dir->entries[i].nodeID);
                return OK;
            }
        }

        return OK;
    }

    static CachedNode* CreateNode(MountPoint* mp, uint64 softRefs = 1) {
        CachedNode* newNode = new CachedNode();
        mp->fs->CreateNode(&newNode->node);
        newNode->nodeID = newNode->node.id;
        newNode->softRefCount = softRefs;
        newNode->node.lock.SetLocked();

        mp->nodeCacheLock.Spinlock();
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

    const char* ErrorToString(int64 error) {
        switch(error) {
        case OK: return "OK";
        case ErrorFileNotFound: return "File not found";
        case ErrorInvalidBuffer: return "Invalid user buffer";
        case ErrorInvalidFD: return "Invalid file descriptor";
        case ErrorInvalidFileSystem: return "Invalid file system id";
        case ErrorInvalidPath: return "Invalid path";
        case ErrorPermissionDenied: return "Permission denied";
        case ErrorInvalidDevice: return "Invalid device";
        default: return "unknown error";
        }
    }

    int64 CreateFile(User* user, const char* path, const Permissions& perms) {
        char cleanBuffer[255];
        if(!CleanPath(path, cleanBuffer))
            return ErrorInvalidPath;

        const char* folderPath;
        const char* fileName;
        SplitFolderAndFile(cleanBuffer, &folderPath, &fileName);

        MountPoint* mp = AcquireMountPoint(folderPath);
        folderPath = AdvancePath(folderPath, mp->path);

        CachedNode* folderNode;
        CachedNode* fileNode;
        int64 error = AcquirePathAndParent(user, mp, folderPath, fileName, &folderNode, &fileNode);
        if(error != OK)
            return error;
        if(fileNode != nullptr) {
            ReleaseNode(mp, folderNode);
            ReleaseNode(mp, fileNode);
            return ErrorFileExists;
        }
        if(!CheckPermissions(user, &folderNode->node, Permissions::Write)) {
            ReleaseNode(mp, folderNode);
            return ErrorPermissionDenied;
        }

        GetDir(&folderNode->node);
        DirectoryEntry* newEntry;
        Directory::AddEntry(&folderNode->node.dir, &newEntry);

        CachedNode* newNode = CreateNode(mp);
        newNode->node.linkRefCount = 1;
        newNode->node.type = Node::TYPE_FILE;
        newNode->node.ownerGID = user->gid;
        newNode->node.ownerUID = user->uid;
        newNode->node.permissions = perms;
        
        newEntry->nodeID = newNode->nodeID;
        kstrcpy(newEntry->name, fileName);

        ReleaseNode(mp, folderNode);
        ReleaseNode(mp, newNode);

        return OK;
    }

    int64 CreateFolder(User* user, const char* path, const Permissions& perms) {
        char cleanBuffer[255];
        if(!CleanPath(path, cleanBuffer))
            return ErrorInvalidPath;

        const char* folderPath;
        const char* fileName;
        SplitFolderAndFile(cleanBuffer, &folderPath, &fileName);

        MountPoint* mp = AcquireMountPoint(folderPath);
        folderPath = AdvancePath(folderPath, mp->path);

        CachedNode* folderNode = nullptr;
        CachedNode* fileNode = nullptr;
        int64 error = AcquirePathAndParent(user, mp, folderPath, fileName, &folderNode, &fileNode);
        if(error != OK)
            return error;
        if(fileNode != nullptr) {
            ReleaseNode(mp, folderNode);
            ReleaseNode(mp, fileNode);
            return ErrorFileExists;
        }
        if(!CheckPermissions(user, &folderNode->node, Permissions::Write)) {
            ReleaseNode(mp, folderNode);
            return ErrorPermissionDenied;
        }

        GetDir(&folderNode->node);
        DirectoryEntry* newEntry;
        Directory::AddEntry(&folderNode->node.dir, &newEntry);

        CachedNode* newNode = CreateNode(mp);
        newNode->node.linkRefCount = 1;
        newNode->node.type = Node::TYPE_DIRECTORY;
        newNode->node.dir = Directory::Create(10);
        newNode->node.ownerGID = user->gid;
        newNode->node.ownerUID = user->uid;
        newNode->node.permissions = perms;
        
        newEntry->nodeID = newNode->nodeID;
        kstrcpy(newEntry->name, fileName);

        ReleaseNode(mp, folderNode);
        ReleaseNode(mp, newNode);

        return OK;
    }

    int64 CreateDeviceFile(User* user, const char* path, const Permissions& perms, uint64 driverID, uint64 subID) {
        char cleanBuffer[255];
        if(!CleanPath(path, cleanBuffer))
            return ErrorInvalidPath;

        const char* folderPath;
        const char* fileName;
        SplitFolderAndFile(cleanBuffer, &folderPath, &fileName);

        MountPoint* mp = AcquireMountPoint(folderPath);
        folderPath = AdvancePath(folderPath, mp->path);

        CachedNode* folderNode = nullptr;
        CachedNode* fileNode = nullptr;
        int64 error = AcquirePathAndParent(user, mp, folderPath, fileName, &folderNode, &fileNode);
        if(error != OK)
            return error;
        if(fileNode != nullptr) {
            ReleaseNode(mp, folderNode);
            ReleaseNode(mp, fileNode);
            return ErrorFileExists;
        }
        if(!CheckPermissions(user, &folderNode->node, Permissions::Write)) {
            ReleaseNode(mp, folderNode);
            return ErrorPermissionDenied;
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
        newNode->node.ownerGID = user->gid;
        newNode->node.ownerUID = user->uid;
        newNode->node.permissions = perms;
        
        newEntry->nodeID = newNode->nodeID;
        kstrcpy(newEntry->name, fileName);

        ReleaseNode(mp, folderNode);
        ReleaseNode(mp, newNode);

        return OK;
    }
    int64 CreatePipe(User* user, uint64* readDesc, uint64* writeDesc) {
        CachedNode* pipeNode = CreateNode(g_PipeMount, 2);
        pipeNode->node.type = Node::TYPE_PIPE;
        ReleaseNode(g_PipeMount, pipeNode, false);

        FileDescriptor* descRead = new FileDescriptor();
        descRead->mp = g_PipeMount;
        descRead->node = pipeNode;
        descRead->pos = 0;
        descRead->refCount = 1;
        descRead->permissions = Permissions::Read;

        FileDescriptor* descWrite = new FileDescriptor();
        descWrite->mp = g_PipeMount;
        descWrite->node = pipeNode;
        descWrite->pos = 0;
        descWrite->refCount = 1;
        descWrite->permissions = Permissions::Write;

        *readDesc = (uint64)descRead;
        *writeDesc = (uint64)descWrite;

        return OK;
    }

    int64 Delete(User* user, const char* path) {
        char cleanBuffer[255];
        if(!CleanPath(path, cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp = AcquireMountPoint(cleanBuffer);
        char* relPath = (char*)AdvancePath(cleanBuffer, mp->path);
        if(*relPath == '\0') // trying to delete a mountpoint
            return ErrorPermissionDenied;
        
        const char* folderPath;
        const char* fileName;
        SplitFolderAndFile(relPath, &folderPath, &fileName);

        CachedNode* folderNode;
        int64 error = AcquirePath(user, mp, folderPath, &folderNode);
        if(error != OK)
            return error;
        if(folderNode->node.type != Node::TYPE_DIRECTORY) {
            ReleaseNode(mp, folderNode);
            return ErrorFileNotFound;
        } 
        if(!CheckPermissions(user, &folderNode->node, Permissions::Write)) {
            ReleaseNode(mp, folderNode);
            return ErrorPermissionDenied;
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
                        return ErrorPermissionDenied;
                    }
                }

                Directory::RemoveEntry(&folderNode->node.dir, i);
                fileNode->node.linkRefCount--;

                ReleaseNode(mp, fileNode);
                ReleaseNode(mp, folderNode);
                return OK;
            }
        }

        ReleaseNode(mp, folderNode);
        return ErrorFileNotFound;
    }

    int64 ChangeOwner(User* user, const char* path, uint64 newUID, uint64 newGID) {
        char cleanBuffer[255];
        if(!CleanPath(path, cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp = AcquireMountPoint(cleanBuffer);
        path = AdvancePath(cleanBuffer, mp->path);

        CachedNode* node;
        int64 error = AcquirePath(user, mp, path, &node);
        if(error != OK)
            return error;
        if(user->uid != 0 && user->uid != node->node.ownerUID) {
            ReleaseNode(mp, node);
            return ErrorPermissionDenied;
        }
        
        node->node.ownerUID = newUID;
        node->node.ownerGID = newGID;
        ReleaseNode(mp, node);

        return OK;
    }

    int64 ChangePermissions(User* user, const char* path, const Permissions& permissions) {
        char cleanBuffer[255];
        if(!CleanPath(path, cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp = AcquireMountPoint(cleanBuffer);
        path = AdvancePath(cleanBuffer, mp->path);

        CachedNode* node;
        int64 error = AcquirePath(user, mp, path, &node);
        if(error != OK)
            return error;
        if(user->uid != 0 && user->uid != node->node.ownerUID) {
            ReleaseNode(mp, node);
            return ErrorPermissionDenied;
        }
        
        node->node.permissions = permissions;
        ReleaseNode(mp, node);

        return OK;
    }

    int64 Mount(User* user, const char* mountPoint, FileSystem* fs) {
        char cleanBuffer[255];
        if(!CleanPath(mountPoint, cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp = AcquireMountPoint(cleanBuffer);
        const char* folderPath = AdvancePath(cleanBuffer, mp->path);
        if(*folderPath == '\0') // Trying to mount onto mountpoint
            return ErrorPermissionDenied;

        CachedNode* folderNode;
        int64 error = AcquirePath(user, mp, folderPath, &folderNode);
        if(error != OK)
            return error;
        if(folderNode->node.type != Node::TYPE_DIRECTORY) {
            ReleaseNode(mp, folderNode);
            return ErrorFileNotFound;
        } 
        if(!CheckPermissions(user, &folderNode->node, Permissions::Write)) {
            ReleaseNode(mp, folderNode);
            return ErrorPermissionDenied;
        }
        
        Directory* dir = GetDir(&folderNode->node);
        if(dir->numEntries != 0) {
            ReleaseNode(mp, folderNode);
            return ErrorPermissionDenied;
        }

        MountPoint* newMP = new MountPoint();
        kstrcpy(newMP->path, cleanBuffer);
        newMP->fs = fs;
        fs->GetSuperBlock(&newMP->sb);
        
        mp->childMountLock.Spinlock();
        mp->childMounts.push_back(newMP);
        mp->childMountLock.Unlock();

        return OK;
    }
    int64 Mount(User* user, const char* mountPoint, const char* fsID) {
        FileSystem* fs = FileSystemRegistry::CreateFileSystem(fsID);
        if(fs == nullptr)
            return ErrorInvalidFileSystem;

        int64 error = Mount(user, mountPoint, fs);
        if(error != OK) {
            delete fs;
            return error;
        }

        return OK;
    }
    int64 Mount(User* user, const char* mountPoint, const char* fsID, const char* devFile) {
        char cleanBuffer[255];
        if(!CleanPath(devFile, cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp = AcquireMountPoint(cleanBuffer);
        devFile = AdvancePath(cleanBuffer, mp->path);

        CachedNode* node;
        int64 error = AcquirePath(user, mp, devFile, &node);
        if(error != OK)
            return error;
        if(node->node.type != Node::TYPE_DEVICE_BLOCK) {
            ReleaseNode(mp, node);
            return ErrorInvalidDevice;
        }
        if(!CheckPermissions(user, &node->node, Permissions::Read)) {
            ReleaseNode(mp, node);
            return ErrorPermissionDenied;
        }
        
        uint64 driverID = node->node.device.driverID;
        uint64 subID = node->node.device.subID;

        ReleaseNode(mp, node);

        return Mount(user, mountPoint, fsID, driverID, subID);
    }
    int64 Mount(User* user, const char* mountPoint, const char* fsID, uint64 driverID, uint64 devID) {
        DeviceDriver* driver = DeviceDriverRegistry::GetDriver(driverID);
        if(driver->GetType() != DeviceDriver::TYPE_BLOCK)
            return ErrorInvalidDevice;

        FileSystem* fs = FileSystemRegistry::CreateFileSystem(fsID, (BlockDeviceDriver*)driver, devID);
        if(fs == nullptr)
            return ErrorInvalidFileSystem;

        int64 error = Mount(user, mountPoint, fs);
        if(error != OK) {
            delete fs;
        }

        return error;
    }

    int64 Unmount(User* user, const char* mountPoint) {
        return OK;
    }

    int64 Open(User* user, const char* path, uint64 openMode, uint64& fileDesc) {
        char cleanBuffer[255];
        if(!CleanPath(path, cleanBuffer))
            return ErrorInvalidPath;

        const char* folderPath;
        const char* fileName;
        SplitFolderAndFile(cleanBuffer, &folderPath, &fileName);

        MountPoint* mp = AcquireMountPoint(folderPath);
        folderPath = AdvancePath(folderPath, mp->path);

        CachedNode* parent = nullptr;
        CachedNode* file = nullptr;
        int64 error = AcquirePathAndParent(user, mp, folderPath, fileName, &parent, &file);
        if(error != OK)
            return error;
            
        if(file == nullptr) {
            if(openMode & OpenMode_Create) {
                if(!CheckPermissions(user, &parent->node, Permissions::Write)) {
                    ReleaseNode(mp, parent);
                    return ErrorPermissionDenied;
                }

                file = CreateNode(mp);
                file->node.linkRefCount = 1;
                file->node.type = Node::TYPE_FILE;
                file->node.ownerGID = user->gid;
                file->node.ownerUID = user->uid;
                file->node.permissions = parent->node.permissions;

                GetDir(&parent->node);
                DirectoryEntry* newEntry;
                Directory::AddEntry(&parent->node.dir, &newEntry);
                newEntry->nodeID = file->nodeID;
                kstrcpy(newEntry->name, fileName);
            } else {
                ReleaseNode(mp, parent);
                return ErrorFileNotFound;
            }
        }
        
        ReleaseNode(mp, parent);

        if(openMode & OpenMode_Clear) {
            // TODO: implement
        }

        ReleaseNode(mp, file, false);

        FileDescriptor* desc = new FileDescriptor();
        desc->mp = mp;
        desc->node = file;
        desc->pos = 0;
        desc->refCount = 1;
        desc->permissions = openMode & 0xFF;

        fileDesc = (uint64)desc;
        return OK;
    }

    int64 Close(uint64 descID) {
        FileDescriptor* desc = (FileDescriptor*)descID;
        if(desc == nullptr)
            return ErrorInvalidFD;

        desc->refCount--;
        if(desc->refCount == 0) {
            CachedNode* node = desc->node;
            node->node.lock.Lock();
            ReleaseNode(desc->mp, node);

            delete desc;
        }

        return OK;
    }

    int64 AddRef(uint64 descID) {
        FileDescriptor* desc = (FileDescriptor*)descID;
        if(desc == nullptr)
            return ErrorInvalidFD;

        desc->refCount++;

        return OK;
    }

    int64 List(User* user, const char* path, FileList* list, bool getTypes) {
        char cleanBuffer[255];
        if(!CleanPath(path, cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp = AcquireMountPoint(cleanBuffer);
        path = AdvancePath(cleanBuffer, mp->path);

        CachedNode* node;
        int64 error = AcquirePath(user, mp, path, &node);
        if(error != OK)
            return error;
        if(node->node.type != Node::TYPE_DIRECTORY || !CheckPermissions(user, &node->node, Permissions::Read)) {
            ReleaseNode(mp, node);
            return ErrorPermissionDenied;
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
        return OK;
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

            uint64 error = driver->GetData(devID, pos, buffer, bufferSize);
            if(error != 0)
                return error;
            res = bufferSize;
        } else {
            res = node->fs->ReadNodeData(node, pos, buffer, bufferSize);
        }
        return res;
    }

    int64 Read(uint64 descID, void* buffer, uint64 bufferSize) {
        FileDescriptor* desc = (FileDescriptor*)descID;
        if(desc == nullptr)
            return ErrorInvalidFD;

        if(!(desc->permissions & Permissions::Read))
            return ErrorPermissionDenied;
        
        int64 res = _Read(&desc->node->node, desc->pos, buffer, bufferSize);
        if(res >= 0)
            desc->pos += res;

        return res;
    }

    int64 Read(uint64 descID, uint64 pos, void* buffer, uint64 bufferSize) {
        FileDescriptor* desc = (FileDescriptor*)descID;
        if(desc == nullptr)
            return ErrorInvalidFD;

        if(!(desc->permissions & Permissions::Read))
            return ErrorPermissionDenied;

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
            BlockDeviceDriver* driver = (BlockDeviceDriver*)DeviceDriverRegistry::GetDriver(node->device.driverID);
            if(driver == nullptr)
                return 0;

            uint64 devID = node->device.subID;
            uint64 error = driver->SetData(devID, pos, buffer, bufferSize);
            if(error != 0)
                return error;
            res = bufferSize;
        } else {
            res = node->fs->WriteNodeData(node, pos, buffer, bufferSize);
        }
        return res;
    }

    int64 Write(uint64 descID, const void* buffer, uint64 bufferSize) {
        FileDescriptor* desc = (FileDescriptor*)descID;
        if(desc == nullptr)
            return ErrorInvalidFD;

        if(!(desc->permissions & Permissions::Write))
            return ErrorPermissionDenied;
        
        int64 res = _Write(&desc->node->node, desc->pos, buffer, bufferSize);
        if(res >= 0)
            desc->pos += res;

        return res;
    }

    int64 Write(uint64 descID, uint64 pos, const void* buffer, uint64 bufferSize) {
        FileDescriptor* desc = (FileDescriptor*)descID;
        if(desc == nullptr)
            return ErrorInvalidFD;

        if(!(desc->permissions & Permissions::Write))
            return ErrorPermissionDenied;

        return _Write(&desc->node->node, pos, buffer, bufferSize);
    }

    int64 Stat(uint64 descID, NodeStats* stats) {
        FileDescriptor* desc = (FileDescriptor*)descID;
        if(desc == nullptr)
            return ErrorInvalidFD;

        stats->size = desc->node->node.fileSize;
        stats->ownerGID = desc->node->node.ownerGID;
        stats->ownerUID = desc->node->node.ownerUID;
        stats->permissions = desc->node->node.permissions;
        return OK;
    }

}