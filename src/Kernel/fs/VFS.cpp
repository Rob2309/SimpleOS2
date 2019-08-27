#include "VFS.h"

#include "ktl/list.h"
#include "klib/string.h"
#include "klib/memory.h"
#include "klib/stdio.h"
#include "devices/DeviceDriver.h"
#include "PipeFS.h"

#include "syscalls/SyscallDefine.h"
#include "memory/MemoryManager.h"
#include "scheduler/Scheduler.h"
#include "scheduler/Process.h"

#include "MountPoint.h"
#include "FileSystem.h"

namespace VFS {

    struct FileDescriptor {
        uint64 refCount;

        Node* node;

        uint8 permissions;

        Atomic<uint64> pos;
    };

    static MountPoint* g_RootMount = nullptr;
    static MountPoint* g_PipeMount = nullptr;

    static bool CleanPath(char* cleanBuffer) {
        int length = kstrlen(cleanBuffer);
        
        if(cleanBuffer[0] != '/')
            return false;

        int writePos = 1;
        for(int i = 1; i < length; i++) {
            char c = cleanBuffer[i];
            if(c == '/' && cleanBuffer[writePos - 1] == '/') {
            } else {
                cleanBuffer[writePos] = c;
                writePos++;
            }
        }

        if(cleanBuffer[writePos-1] == '/')
            cleanBuffer[writePos - 1] = '\0';
        else
            cleanBuffer[writePos] = '\0';

        return true;
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

    static MountPoint* FindMountPoint(const char* path) {
        MountPoint* current = g_RootMount;
        current->childMountLock.Spinlock();

        while(true) {
            MountPoint* next = nullptr;
            for(MountPoint* mp : current->childMounts) {
                if(MountCmp(path, mp->path)) {
                    mp->refCount.Inc();
                    next = mp;
                    break;
                }
            }

            if(next != nullptr) {
                current->childMountLock.Unlock();
                if(current != g_RootMount)
                    current->refCount.Dec();
                next->childMountLock.Spinlock();
                current = next;
            } else {
                current->childMountLock.Unlock();
                return current;
            }
        }
    }
    static void ReleaseMountPoint(MountPoint* mp, bool decrement = true) {
        if(mp != g_RootMount && decrement)
            mp->refCount.Dec();
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

    static Node* AcquireNode(MountPoint* mp, uint64 nodeID) {
        mp->nodeCacheLock.Spinlock();
        for(auto n : mp->nodeCache) {
            if(n->id == nodeID) {
                n->refCount++;
                mp->nodeCacheLock.Unlock();
                
                if(!n->ready) {
                    n->readyQueue.Lock();   // wait for node to be ready
                    n->readyQueue.Unlock(); // notify next waiting thread
                }

                return n;
            }
        }

        auto newNode = new Node();
        newNode->id = nodeID;
        newNode->mp = mp;
        newNode->refCount = 1;
        newNode->ready = false;
        newNode->readyQueue.Lock();
        newNode->dirLock.Unlock_Raw();
        mp->nodeCache.push_back(newNode);
        mp->nodeCacheLock.Unlock();

        mp->fs->ReadNode(nodeID, newNode);
        newNode->ready = true;
        newNode->readyQueue.Unlock(); // notify first waiting thread
        return newNode;
    }

    static void ReleaseNode(Node* node, bool decrement = true) {
        node->mp->nodeCacheLock.Spinlock();

        if(decrement)
            node->refCount--;

        // Since linkCount only ever changes if refCount != 0, and refCount cannot change, this test is safe
        if(node->refCount == 0 && node->linkCount.Read() == 0) {
            node->mp->nodeCache.erase(node);
            node->mp->nodeCacheLock.Unlock();

            node->mp->fs->DestroyNode(node);
            delete node;
        } else {
            node->mp->nodeCacheLock.Unlock();
        }
    }

    void ReplaceDirEntries(MountPoint* mp, uint64 nodeID, Directory* newDir) {
        mp->nodeCacheLock.Spinlock();
        for(auto n : mp->nodeCache) {
            if(n->id == nodeID) {
                n->refCount++;
                mp->nodeCacheLock.Unlock();

                n->dirLock.Spinlock();
                auto dir = n->infoFolder.dir;
                n->infoFolder.dir = newDir;
                n->dirLock.Unlock();
                if(dir != nullptr)
                    Directory::Destroy(dir);

                ReleaseNode(n);
                return;
            }
        }
        mp->nodeCacheLock.Unlock();
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

    static bool IsLastPathEntry(const char* path) {
        while(*path != '\0') {
            if(*path == '/')
                return false;
            path++;
        }
        return true;
    }

    static const char* GetFileName(const char* path) {
        int lastSep = -1;
        int index = 0;
        while(path[index] != '\0') {
            if(path[index] == '/')
                lastSep = index;
            index++;
        }
        return &path[lastSep + 1];
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

    static int64 _AcquirePathRec(User* user, MountPoint* mp, const char*& pathBuffer, bool fileHasToExist, Node*& outNode, Node*& outParent) {
        auto currentNode = AcquireNode(mp, mp->sb.rootNode);

        if(pathBuffer[0] == '\0') {
            outNode = currentNode;
            outParent = nullptr;
            return OK;
        }

        while(true) {
            if(currentNode->type != Node::TYPE_DIRECTORY) {
                ReleaseNode(currentNode);
                return ErrorFileNotFound;
            }
            if(!CheckPermissions(user, currentNode, Permissions::Read)) {
                ReleaseNode(currentNode);
                return ErrorPermissionDenied;
            }

            currentNode->dirLock.Spinlock();
            Node* next = nullptr;
            for(uint64 i = 0; i < currentNode->infoFolder.dir->numEntries; i++) {
                if(PathWalk(&pathBuffer, currentNode->infoFolder.dir->entries[i].name)) {
                    uint64 nid = currentNode->infoFolder.dir->entries[i].nodeID;
                    currentNode->dirLock.Unlock();
                    next = AcquireNode(mp, nid);

                    if(next->type == Node::TYPE_SYMLINK) {
                        outNode = next;
                        outParent = currentNode;
                        return ErrorEncounteredSymlink;
                    }

                    if(*pathBuffer == '\0') {
                        outNode = next;
                        outParent = currentNode;
                        return OK;
                    }

                    break;
                }
            }

            if(next == nullptr) {
                currentNode->dirLock.Unlock();
                if(fileHasToExist || !IsLastPathEntry(pathBuffer)) {
                    ReleaseNode(currentNode);
                    return ErrorFileNotFound;
                } else {
                    outParent = currentNode;
                    outNode = nullptr;
                    return OK;
                }
            } else {
                ReleaseNode(currentNode);
                currentNode = next;
            }
        }
    }

    /**
     * If outParent is nullptr, outNode has to be a mountPoint
     **/
    static int64 AcquirePath(User* user, MountPoint*& outMP, char*& inOutPathBuffer, bool fileHasToExist, Node*& outNode, Node*& outParent) {
        MountPoint* mp = FindMountPoint(inOutPathBuffer);
        const char* currentPath = AdvancePath(inOutPathBuffer, mp->path);

        int64 error;
        while((error = _AcquirePathRec(user, mp, currentPath, fileHasToExist, outNode, outParent)) == ErrorEncounteredSymlink) {
            const char* linkPath = outNode->infoSymlink.linkPath;
            const char* restPath = currentPath;

            int lA = kstrlen(linkPath);
            int lB = kstrlen(restPath);
            int l = lA + lB + 2;
            if(l > 255) {
                ReleaseNode(outNode);
                ReleaseNode(outParent);
                ReleaseMountPoint(mp);
                return ErrorInvalidPath;
            }

            if(lB > 0) {
                kmemmove(inOutPathBuffer + lA + 1, restPath, lB + 1);
                kmemcpy(inOutPathBuffer, linkPath, lA);
                inOutPathBuffer[lA] = '/';
            } else {
                kmemcpy(inOutPathBuffer, linkPath, lA + 1);
            }

            ReleaseNode(outNode);
            ReleaseNode(outParent);

            ReleaseMountPoint(mp);
            mp = FindMountPoint(inOutPathBuffer);
            currentPath = AdvancePath(inOutPathBuffer, mp->path);
        }

        if(error == OK) {
            outMP = mp;
            inOutPathBuffer = (char*)currentPath;
            return OK;
        } else {
            ReleaseMountPoint(mp);
            return error;
        }
    }

    static Node* CreateNode(MountPoint* mp, uint64 softRefs = 1) {
        auto newNode = new Node();
        mp->fs->CreateNode(newNode);
        newNode->refCount = softRefs;
        newNode->mp = mp;
        newNode->dirLock.Unlock_Raw();

        mp->nodeCacheLock.Spinlock();
        mp->nodeCache.push_back(newNode);
        mp->nodeCacheLock.Unlock();

        return newNode;
    }

    void Init(FileSystem* rootFS) {
        auto mp = new MountPoint();
        mp->fs = rootFS;
        mp->refCount = 2; // Can never be unmounted again
        rootFS->GetSuperBlock(&mp->sb);
        rootFS->SetMountPoint(mp);
        kstrcpy(mp->path, "/");
        g_RootMount = mp;

        g_PipeMount = new MountPoint();
        g_PipeMount->fs = new PipeFS();
    }

    int64 CreateFile(User* user, const char* path, const Permissions& perms) {
        char cleanBuffer[255];
        if(!kpathcpy_usersafe(cleanBuffer, path))
            return ErrorInvalidBuffer;
        if(!CleanPath(cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp;
        char* tmpPath = cleanBuffer;
        Node* parentNode;
        Node* fileNode;
        int64 error = AcquirePath(user, mp, tmpPath, false, fileNode, parentNode);
        if(error != OK)
            return error;
        if(parentNode == nullptr) {
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorFileExists;
        }
        if(fileNode != nullptr) {
            ReleaseNode(parentNode);
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorFileExists;
        }
        if(!CheckPermissions(user, parentNode, Permissions::Write)) {
            ReleaseNode(parentNode);
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }

        auto newNode = CreateNode(mp);
        newNode->linkCount.Write(1);
        newNode->type = Node::TYPE_FILE;
        newNode->infoFile.fileSize = 0;
        newNode->ownerGID = user->gid;
        newNode->ownerUID = user->uid;
        newNode->permissions = perms;

        parentNode->dirLock.Spinlock();
        DirectoryEntry* newEntry;
        Directory::AddEntry(&parentNode->infoFolder.dir, &newEntry);
        
        newEntry->nodeID = newNode->id;
        kstrcpy(newEntry->name, tmpPath);
        parentNode->dirLock.Unlock();

        ReleaseNode(parentNode);
        ReleaseNode(newNode);
        ReleaseMountPoint(mp);

        return OK;
    }
    SYSCALL_DEFINE1(syscall_create_file, const char* filePath) {
        if(!MemoryManager::IsUserPtr(filePath))
            Scheduler::ThreadKillProcess("InvalidUserPointer");

        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
        ProcessInfo* pInfo = tInfo->process;

        return CreateFile(pInfo->owner, filePath, {3, 3, 3});
    }

    int64 CreateFolder(User* user, const char* path, const Permissions& perms) {
        char cleanBuffer[255];
        if(!kpathcpy_usersafe(cleanBuffer, path))
            return ErrorInvalidBuffer;
        if(!CleanPath(cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp;
        char* tmpPath = cleanBuffer;
        Node* parentNode;
        Node* fileNode;
        int64 error = AcquirePath(user, mp, tmpPath, false, fileNode, parentNode);
        if(error != OK)
            return error;
        if(parentNode == nullptr) {
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorFileExists;
        }
        if(fileNode != nullptr) {
            ReleaseNode(parentNode);
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorFileExists;
        }
        if(!CheckPermissions(user, parentNode, Permissions::Write)) {
            ReleaseNode(parentNode);
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }

        auto newNode = CreateNode(mp);
        newNode->linkCount.Write(1);
        newNode->type = Node::TYPE_DIRECTORY;
        newNode->infoFolder.dir = Directory::Create(10);
        newNode->ownerGID = user->gid;
        newNode->ownerUID = user->uid;
        newNode->permissions = perms;

        parentNode->dirLock.Spinlock();
        DirectoryEntry* newEntry;
        Directory::AddEntry(&parentNode->infoFolder.dir, &newEntry);
        
        newEntry->nodeID = newNode->id;
        kstrcpy(newEntry->name, tmpPath);
        parentNode->dirLock.Unlock();

        ReleaseNode(parentNode);
        ReleaseNode(newNode);
        ReleaseMountPoint(mp);

        return OK;
    }
    SYSCALL_DEFINE1(syscall_create_folder, const char* filePath) {
        if(!MemoryManager::IsUserPtr(filePath))
            Scheduler::ThreadKillProcess("InvalidUserPointer");

        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
        ProcessInfo* pInfo = tInfo->process;

        return CreateFolder(pInfo->owner, filePath, {3, 3, 3});
    }

    int64 CreateDeviceFile(User* user, const char* path, const Permissions& perms, uint64 driverID, uint64 subID) {
        char cleanBuffer[255];
        if(!kpathcpy_usersafe(cleanBuffer, path))
            return ErrorInvalidBuffer;
        if(!CleanPath(cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp;
        char* tmpPath = cleanBuffer;
        Node* parentNode;
        Node* fileNode;
        int64 error = AcquirePath(user, mp, tmpPath, false, fileNode, parentNode);
        if(error != OK)
            return error;
        if(parentNode == nullptr) {
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorFileExists;
        }
        if(fileNode != nullptr) {
            ReleaseNode(parentNode);
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorFileExists;
        }
        if(!CheckPermissions(user, parentNode, Permissions::Write)) {
            ReleaseNode(parentNode);
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }

        auto driver = DeviceDriverRegistry::GetDriver(driverID);

        auto newNode = CreateNode(mp);
        newNode->linkCount.Write(1);
        newNode->type = driver->GetType() == DeviceDriver::TYPE_BLOCK ? Node::TYPE_DEVICE_BLOCK : Node::TYPE_DEVICE_CHAR;
        newNode->infoDevice.driverID = driverID;
        newNode->infoDevice.subID = subID;
        newNode->ownerGID = user->gid;
        newNode->ownerUID = user->uid;
        newNode->permissions = perms;

        parentNode->dirLock.Spinlock();
        DirectoryEntry* newEntry;
        Directory::AddEntry(&parentNode->infoFolder.dir, &newEntry);
        
        newEntry->nodeID = newNode->id;
        kstrcpy(newEntry->name, tmpPath);
        parentNode->dirLock.Unlock();

        ReleaseNode(parentNode);
        ReleaseNode(newNode);
        ReleaseMountPoint(mp);

        return OK;
    }
    SYSCALL_DEFINE3(syscall_create_dev, const char* filePath, uint64 driverID, uint64 devID) {
        if(!MemoryManager::IsUserPtr(filePath))
            Scheduler::ThreadKillProcess("InvalidUserPointer");

        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
        ProcessInfo* pInfo = tInfo->process;

        return CreateDeviceFile(pInfo->owner, filePath, {3, 3, 3}, driverID, devID);
    }

    int64 CreatePipe(User* user, uint64* readDesc, uint64* writeDesc) {
        auto pipeNode = CreateNode(g_PipeMount, 2);
        pipeNode->type = Node::TYPE_PIPE;

        FileDescriptor* descRead = new FileDescriptor();
        descRead->node = pipeNode;
        descRead->pos = 0;
        descRead->refCount = 1;
        descRead->permissions = Permissions::Read;

        FileDescriptor* descWrite = new FileDescriptor();
        descWrite->node = pipeNode;
        descWrite->pos = 0;
        descWrite->refCount = 1;
        descWrite->permissions = Permissions::Write;

        *readDesc = (uint64)descRead;
        *writeDesc = (uint64)descWrite;

        return OK;
    }
    SYSCALL_DEFINE2(syscall_pipe, int64* readDesc, int64* writeDesc) {
        uint64 sysRead, sysWrite;

        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
        ProcessInfo* pInfo = tInfo->process;

        VFS::CreatePipe(pInfo->owner, &sysRead, &sysWrite);
        *readDesc = Scheduler::ProcessAddFileDescriptor(sysRead);
        *writeDesc = Scheduler::ProcessAddFileDescriptor(sysWrite);
        return 0;
    }

    int64 CreateSymLink(User* user, const char* path, const Permissions& permissions, const char* linkPath) {
        char cleanBuffer[255];
        if(!kpathcpy_usersafe(cleanBuffer, path))
            return ErrorInvalidBuffer;
        if(!CleanPath(cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp;
        char* tmpPath = cleanBuffer;
        Node* parentNode;
        Node* linkNode;
        int64 error = AcquirePath(user, mp, tmpPath, false, linkNode, parentNode);
        if(error != OK)
            return error;
        if(parentNode == nullptr) {
            ReleaseNode(linkNode);
            ReleaseMountPoint(mp);
            return ErrorFileExists;
        }
        if(linkNode != nullptr) {
            ReleaseNode(parentNode);
            ReleaseNode(linkNode);
            ReleaseMountPoint(mp);
            return ErrorFileExists;
        }
        if(!CheckPermissions(user, parentNode, Permissions::Write)) {
            ReleaseNode(parentNode);
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }

        auto newNode = CreateNode(mp);
        newNode->linkCount.Write(1);
        newNode->type = Node::TYPE_SYMLINK;
        newNode->infoSymlink.linkPath = new char[kstrlen(linkPath) + 1];
        kmemcpy(newNode->infoSymlink.linkPath, linkPath, kstrlen(linkPath) + 1);
        newNode->ownerUID = user->uid;
        newNode->ownerGID = user->gid;
        newNode->permissions = permissions;

        parentNode->dirLock.Spinlock();
        DirectoryEntry* newEntry;
        Directory::AddEntry(&parentNode->infoFolder.dir, &newEntry);
        
        newEntry->nodeID = newNode->id;
        kstrcpy(newEntry->name, tmpPath);
        parentNode->dirLock.Unlock();

        ReleaseNode(parentNode);
        ReleaseNode(newNode);
        ReleaseMountPoint(mp);

        return OK;
    }
    SYSCALL_DEFINE2(syscall_create_symlink, const char* path, const char* linkPath) {
        if(!MemoryManager::IsUserPtr(path) || !MemoryManager::IsUserPtr(linkPath))
            Scheduler::ThreadKillProcess("InvalidUserPointer");

        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
        ProcessInfo* pInfo = tInfo->process;

        return CreateSymLink(pInfo->owner, path, { 3, 3, 3 }, linkPath);
    }

    int64 CreateHardLink(User* user, const char* path, const Permissions& permissions, const char* linkPath) {
        char cleanBuffer[255];
        if(!kpathcpy_usersafe(cleanBuffer, linkPath))
            return ErrorInvalidBuffer;
        if(!CleanPath(cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* linkMP;
        char* tmpPath = cleanBuffer;
        Node* linkParentNode;
        Node* linkFileNode;
        int64 error = AcquirePath(user, linkMP, tmpPath, true, linkFileNode, linkParentNode);
        if(error != OK)
            return error;
        if(linkParentNode != nullptr)
            ReleaseNode(linkParentNode);
        // Hardlinks to folders not allowed
        if(linkFileNode->type == Node::TYPE_DIRECTORY) {
            ReleaseNode(linkFileNode);
            ReleaseMountPoint(linkMP);
            return ErrorPermissionDenied;
        }

        if(!kpathcpy_usersafe(cleanBuffer, path)) {
            ReleaseNode(linkFileNode);
            ReleaseMountPoint(linkMP);
            return ErrorInvalidBuffer;
        }
        if(!CleanPath(cleanBuffer)) {
            ReleaseNode(linkFileNode);
            ReleaseMountPoint(linkMP);
            return ErrorInvalidPath;
        }

        MountPoint* mp;
        tmpPath = cleanBuffer;
        Node* parentNode;
        Node* fileNode;
        error = AcquirePath(user, mp, tmpPath, false, fileNode, parentNode);
        if(error != OK) {
            ReleaseNode(linkFileNode);
            ReleaseMountPoint(linkMP);
            return error;
        }
        if(parentNode == nullptr) {
            ReleaseNode(linkFileNode);
            ReleaseMountPoint(linkMP);
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorFileExists;
        }
        if(fileNode != nullptr) {
            ReleaseNode(linkFileNode);
            ReleaseMountPoint(linkMP);
            ReleaseNode(parentNode);
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorFileExists;
        }
        if(!CheckPermissions(user, parentNode, Permissions::Write)) {
            ReleaseNode(linkFileNode);
            ReleaseMountPoint(linkMP);
            ReleaseNode(parentNode);
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }
        // Creating hardlink to different filesystem
        if(mp != linkMP) {
            ReleaseNode(linkFileNode);
            ReleaseMountPoint(linkMP);
            ReleaseNode(parentNode);
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }

        linkFileNode->linkCount.Inc();

        parentNode->dirLock.Spinlock();
        DirectoryEntry* newEntry;
        Directory::AddEntry(&parentNode->infoFolder.dir, &newEntry);

        newEntry->nodeID = linkFileNode->id;
        kstrcpy(newEntry->name, tmpPath);
        parentNode->dirLock.Unlock();

        ReleaseNode(parentNode);
        ReleaseMountPoint(mp);
        ReleaseNode(linkFileNode);
        ReleaseMountPoint(linkMP);
        return OK;
    }
    SYSCALL_DEFINE2(syscall_create_hardlink, const char* path, const char* linkPath) {
        if(!MemoryManager::IsUserPtr(path) || !MemoryManager::IsUserPtr(linkPath))
            Scheduler::ThreadKillProcess("InvalidUserPointer");

        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
        ProcessInfo* pInfo = tInfo->process;

        return CreateHardLink(pInfo->owner, path, { 3, 3, 3 }, linkPath);
    }

    int64 Delete(User* user, const char* path) {
        char cleanBuffer[255];
        if(!kpathcpy_usersafe(cleanBuffer, path))
            return ErrorInvalidBuffer;
        if(!CleanPath(cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp;
        char* tmpPath = cleanBuffer;
        Node* fileNode;
        Node* parentNode;
        int64 error = AcquirePath(user, mp, tmpPath, true, fileNode, parentNode);
        if(error != OK)
            return error;
        // attempting to delete mountpoint
        if(parentNode == nullptr) {
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }
        if(!CheckPermissions(user, parentNode, Permissions::Write)) {
            ReleaseNode(parentNode);
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }
        if(fileNode->type == Node::TYPE_DIRECTORY) {
            fileNode->dirLock.Spinlock();
            if(fileNode->infoFolder.dir->numEntries != 0) {
                fileNode->dirLock.Unlock();
                ReleaseNode(fileNode);
                ReleaseNode(parentNode);
                ReleaseMountPoint(mp);
                return ErrorPermissionDenied;
            }
            fileNode->dirLock.Unlock();
        }

        const char* fileName = GetFileName(cleanBuffer);

        parentNode->dirLock.Spinlock();
        for(uint64 i = 0; i < parentNode->infoFolder.dir->numEntries; i++) {
            if(kstrcmp(fileName, parentNode->infoFolder.dir->entries[i].name) == 0) {
                Directory::RemoveEntry(&parentNode->infoFolder.dir, i);
                parentNode->dirLock.Unlock();
                fileNode->linkCount.Dec();

                ReleaseNode(fileNode);
                ReleaseNode(parentNode);
                ReleaseMountPoint(mp);
                return OK;
            }
        }
    }
    SYSCALL_DEFINE1(syscall_delete, const char* filePath) {
        if(!MemoryManager::IsUserPtr(filePath))
            Scheduler::ThreadKillProcess("InvalidUserPointer");

        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
        ProcessInfo* pInfo = tInfo->process;

        return Delete(pInfo->owner, filePath); 
    }

    int64 ChangeOwner(User* user, const char* path, uint64 newUID, uint64 newGID) {
        char cleanBuffer[255];
        if(!kpathcpy_usersafe(cleanBuffer, path))
            return ErrorInvalidBuffer;
        if(!CleanPath(cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp;
        char* tmpPath = cleanBuffer;
        Node* fileNode;
        Node* folderNode;
        int64 error = AcquirePath(user, mp, tmpPath, true, fileNode, folderNode);
        if(error != OK)
            return error;
        if(folderNode != nullptr)
            ReleaseNode(folderNode);
            ReleaseMountPoint(mp);
        if(user->uid != 0 && user->uid != fileNode->ownerUID) {
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }
        
        fileNode->ownerUID = newUID;
        fileNode->ownerGID = newGID;
        ReleaseNode(fileNode);
        ReleaseMountPoint(mp);

        return OK;
    }
    SYSCALL_DEFINE3(syscall_change_owner, const char* filePath, uint64 uid, uint64 gid) {
        if(!MemoryManager::IsUserPtr(filePath))
            Scheduler::ThreadKillProcess("InvalidUserPointer");

        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
        ProcessInfo* pInfo = tInfo->process;

        return ChangeOwner(pInfo->owner, filePath, uid, gid);
    }

    int64 ChangePermissions(User* user, const char* path, const Permissions& permissions) {
        char cleanBuffer[255];
        if(!kpathcpy_usersafe(cleanBuffer, path))
            return ErrorInvalidBuffer;
        if(!CleanPath(cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp;
        char* tmpPath = cleanBuffer;
        Node* fileNode;
        Node* folderNode;
        int64 error = AcquirePath(user, mp, tmpPath, true, fileNode, folderNode);
        if(error != OK)
            return error;
        if(folderNode != nullptr)
            ReleaseNode(folderNode);
            ReleaseMountPoint(mp);
        if(user->uid != 0 && user->uid != fileNode->ownerUID) {
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }
        
        fileNode->permissions = permissions;
        ReleaseNode(fileNode);
        ReleaseMountPoint(mp);

        return OK;
    }
    SYSCALL_DEFINE4(syscall_change_perm, const char* filePath, uint8 ownerPerm, uint8 groupPerm, uint8 otherPerm) {
        if(!MemoryManager::IsUserPtr(filePath))
            Scheduler::ThreadKillProcess("InvalidUserPointer");

        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
        ProcessInfo* pInfo = tInfo->process;

        return ChangePermissions(pInfo->owner, filePath, { ownerPerm, groupPerm, otherPerm });
    }

    int64 Mount(User* user, const char* mountPoint, FileSystem* fs) {
        char cleanBuffer[255];
        if(!kpathcpy_usersafe(cleanBuffer, mountPoint))
            return ErrorInvalidBuffer;
        if(!CleanPath(cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp;
        char* tmpPath = cleanBuffer;
        Node* folderNode;
        Node* fileNode;
        int64 error = AcquirePath(user, mp, tmpPath, true, fileNode, folderNode);
        if(error != OK)
            return error;
        // Trying to mount onto a mountPoint
        if(folderNode == nullptr) {
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }

        ReleaseNode(folderNode);

        if(fileNode->type != Node::TYPE_DIRECTORY) {
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorFileNotFound;
        } 
        if(!CheckPermissions(user, fileNode, Permissions::Write)) {
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }

        ReleaseNode(fileNode);

        MountPoint* newMP = new MountPoint();
        kstrcpy(newMP->path, cleanBuffer);
        newMP->fs = fs;
        newMP->refCount = 0;
        newMP->parent = mp;
        fs->GetSuperBlock(&newMP->sb);
        fs->SetMountPoint(mp);
        
        mp->childMountLock.Spinlock();
        mp->childMounts.push_back(newMP);
        mp->childMountLock.Unlock();

        ReleaseMountPoint(mp);

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
    SYSCALL_DEFINE2(syscall_mount, const char* mountPoint, const char* fsID) {
        if(!MemoryManager::IsUserPtr(mountPoint) || !MemoryManager::IsUserPtr(fsID))
            Scheduler::ThreadKillProcess("InvalidUserPointer");

        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
        ProcessInfo* pInfo = tInfo->process;

        return Mount(pInfo->owner, mountPoint, fsID); 
    }
    int64 Mount(User* user, const char* mountPoint, const char* fsID, const char* devFile) {
        char cleanBuffer[255];
        if(!kpathcpy_usersafe(cleanBuffer, devFile))
            return ErrorInvalidBuffer;
        if(!CleanPath(cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp;
        char* tmpPath = cleanBuffer;
        Node* folderNode;
        Node* fileNode;
        int64 error = AcquirePath(user, mp, tmpPath, true, fileNode, folderNode);
        if(error != OK)
            return error;
        if(folderNode != nullptr)
            ReleaseNode(folderNode);
        if(fileNode->type != Node::TYPE_DEVICE_BLOCK) {
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorInvalidDevice;
        }
        if(!CheckPermissions(user, fileNode, Permissions::Read)) {
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }
        
        uint64 driverID = fileNode->infoDevice.driverID;
        uint64 subID = fileNode->infoDevice.subID;

        ReleaseNode(fileNode);
        ReleaseMountPoint(mp);

        return Mount(user, mountPoint, fsID, driverID, subID);
    }
    SYSCALL_DEFINE3(syscall_mount_dev, const char* mountPoint, const char* fsID, const char* devFile) {
        if(!MemoryManager::IsUserPtr(mountPoint) || !MemoryManager::IsUserPtr(fsID) || !MemoryManager::IsUserPtr(devFile))
            Scheduler::ThreadKillProcess("InvalidUserPointer");

        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
        ProcessInfo* pInfo = tInfo->process;

        return Mount(pInfo->owner, mountPoint, fsID, devFile);
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
        char cleanBuffer[255];
        if(!kpathcpy_usersafe(cleanBuffer, mountPoint))
            return ErrorInvalidBuffer;
        if(!CleanPath(cleanBuffer))
            return ErrorInvalidPath;

        auto mp = FindMountPoint(cleanBuffer);
        if(mp == g_RootMount) {
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }

        mp->parent->childMountLock.Spinlock();
        if(mp->refCount.Read() == 1) {
            mp->childMountLock.Spinlock();
            if(mp->childMounts.empty()) {
                mp->parent->childMounts.erase(mp);
                mp->parent->childMountLock.Unlock();

                mp->childMountLock.Unlock();

                delete mp;
                return OK;
            } else {
                mp->childMountLock.Unlock();
                mp->parent->childMountLock.Unlock();
                ReleaseMountPoint(mp);
                return ErrorPermissionDenied;
            }
        } else {
            mp->parent->childMountLock.Unlock();
            ReleaseMountPoint(mp);
            return ErrorPermissionDenied;
        }
    }

    int64 Open(User* user, const char* path, uint64 openMode, uint64& fileDesc) {
        char cleanBuffer[255];
        if(!kpathcpy_usersafe(cleanBuffer, path))
            return ErrorInvalidBuffer;
        if(!CleanPath(cleanBuffer))
            return ErrorInvalidPath;

        MountPoint* mp;
        char* tmpPath = cleanBuffer;
        Node* folderNode;
        Node* fileNode;
        int64 error = AcquirePath(user, mp, tmpPath, (openMode & OpenMode_Create) ? false : true, fileNode, folderNode);
        if(error != OK)
            return error;

        if(fileNode == nullptr) {
            if(!CheckPermissions(user, folderNode, Permissions::Write)) {
                ReleaseNode(folderNode);
                ReleaseMountPoint(mp);
                return ErrorPermissionDenied;
            }

            fileNode = CreateNode(mp);
            fileNode->linkCount.Write(1);
            fileNode->type = Node::TYPE_FILE;
            fileNode->infoFile.fileSize.Write(0);
            fileNode->ownerUID = user->uid;
            fileNode->ownerGID = user->gid;
            fileNode->permissions = folderNode->permissions;

            folderNode->dirLock.Spinlock();
            DirectoryEntry* newEntry;
            Directory::AddEntry(&folderNode->infoFolder.dir, &newEntry);
            newEntry->nodeID = fileNode->id;
            kstrcpy(newEntry->name, tmpPath);
            folderNode->dirLock.Unlock();
        } else if(openMode & OpenMode_FailIfExist) {
            if(folderNode != nullptr)
                ReleaseNode(folderNode);
            ReleaseNode(fileNode);
            ReleaseMountPoint(mp);
            return ErrorFileExists;
        }
        
        if(folderNode != nullptr)
            ReleaseNode(folderNode);
        
        if((openMode & OpenMode_Clear) && fileNode->type == Node::TYPE_FILE) {
            mp->fs->ClearNodeData(fileNode);
        }

        ReleaseNode(fileNode, false);
        ReleaseMountPoint(mp, false);

        FileDescriptor* desc = new FileDescriptor();
        desc->node = fileNode;
        desc->pos = 0;
        desc->refCount = 1;
        desc->permissions = openMode & 0xFF;

        fileDesc = (uint64)desc;
        return OK;
    }
    SYSCALL_DEFINE2(syscall_open, const char* filePath, uint64 openMode) {
        ThreadInfo* tInfo = Scheduler::GetCurrentThreadInfo();
        ProcessInfo* pInfo = tInfo->process;

        uint64 sysDesc;
        int64 error = Open(pInfo->owner, filePath, openMode, sysDesc);
        if(error != OK) {
            return error;
        }
        int64 desc = Scheduler::ProcessAddFileDescriptor(sysDesc);
        return desc;
    }

    int64 Close(uint64 descID) {
        FileDescriptor* desc = (FileDescriptor*)descID;
        if(desc == nullptr)
            return ErrorInvalidFD;

        desc->refCount--;
        if(desc->refCount == 0) {
            MountPoint* mp = desc->node->mp;
            Node* node = desc->node;
            ReleaseNode(node);
            ReleaseMountPoint(mp);

            delete desc;
        }

        return OK;
    }
    SYSCALL_DEFINE1(syscall_close, int64 desc) {
        return Scheduler::ProcessCloseFileDescriptor(desc);
    }

    int64 AddRef(uint64 descID) {
        FileDescriptor* desc = (FileDescriptor*)descID;
        if(desc == nullptr)
            return ErrorInvalidFD;

        desc->refCount++;

        return OK;
    }

    static uint64 _Read(Node* node, uint64 pos, void* buffer, uint64 bufferSize) {
        uint64 res;
        if(node->type == Node::TYPE_DEVICE_CHAR) {
            CharDeviceDriver* driver = (CharDeviceDriver*)DeviceDriverRegistry::GetDriver(node->infoDevice.driverID);
            if(driver == nullptr)
                return 0;
            res = driver->Read(node->infoDevice.subID, buffer, bufferSize);
        } else if(node->type == Node::TYPE_DEVICE_BLOCK) {
            BlockDeviceDriver* driver = (BlockDeviceDriver*)DeviceDriverRegistry::GetDriver(node->infoDevice.driverID);
            if(driver == nullptr)
                return 0;
            
            uint64 devID = node->infoDevice.subID;

            uint64 error = driver->GetData(devID, pos, buffer, bufferSize);
            if(error != 0)
                return error;
            res = bufferSize;
        } else {
            res = node->mp->fs->ReadNodeData(node, pos, buffer, bufferSize);
        }
        return res;
    }

    int64 Read(uint64 descID, void* buffer, uint64 bufferSize) {
        FileDescriptor* desc = (FileDescriptor*)descID;
        if(desc == nullptr)
            return ErrorInvalidFD;

        if(!(desc->permissions & Permissions::Read))
            return ErrorPermissionDenied;
        
        uint64 pos = desc->pos.Read();
        int64 res = _Read(desc->node, pos, buffer, bufferSize);
        if(res >= 0)
            desc->pos.Write(pos + res);

        return res;
    }
    SYSCALL_DEFINE3(syscall_read, int64 desc, void* buffer, uint64 bufferSize) {
        if(!MemoryManager::IsUserPtr(buffer))
            Scheduler::ThreadKillProcess("InvalidUserPointer");

        uint64 sysDesc;
        int64 error = Scheduler::ProcessGetSystemFileDescriptor(desc, sysDesc);
        if(error != OK) {
            return error;
        }

        int64 res = VFS::Read(sysDesc, buffer, bufferSize);
        if(res == ErrorInvalidBuffer)
            Scheduler::ThreadKillProcess("InvalidUserPointer");
        return res;
    }

    static uint64 _Write(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) {
        uint64 res;
        if(node->type == Node::TYPE_DEVICE_CHAR) {
            CharDeviceDriver* driver = (CharDeviceDriver*)DeviceDriverRegistry::GetDriver(node->infoDevice.driverID);
            if(driver == nullptr)
                return 0;
            res = driver->Write(node->infoDevice.subID, buffer, bufferSize);
        } else if(node->type == Node::TYPE_DEVICE_BLOCK) {
            BlockDeviceDriver* driver = (BlockDeviceDriver*)DeviceDriverRegistry::GetDriver(node->infoDevice.driverID);
            if(driver == nullptr)
                return 0;

            uint64 devID = node->infoDevice.subID;
            uint64 error = driver->SetData(devID, pos, buffer, bufferSize);
            if(error != 0)
                return error;
            res = bufferSize;
        } else {
            res = node->mp->fs->WriteNodeData(node, pos, buffer, bufferSize);
        }
        return res;
    }

    int64 Write(uint64 descID, const void* buffer, uint64 bufferSize) {
        FileDescriptor* desc = (FileDescriptor*)descID;
        if(desc == nullptr)
            return ErrorInvalidFD;

        if(!(desc->permissions & Permissions::Write))
            return ErrorPermissionDenied;
        
        uint64 pos = desc->pos.Read();
        int64 res = _Write(desc->node, pos, buffer, bufferSize);
        if(res >= 0)
            desc->pos.Write(pos + res);

        return res;
    }
    SYSCALL_DEFINE3(syscall_write, int64 desc, void* buffer, uint64 bufferSize) {
        if(!MemoryManager::IsUserPtr(buffer))
            Scheduler::ThreadKillProcess("InvalidUserPointer");

        uint64 sysDesc;
        int64 error = Scheduler::ProcessGetSystemFileDescriptor(desc, sysDesc);
        if(error != OK) {
            return error;
        }

        int64 res = VFS::Write(sysDesc, buffer, bufferSize);
        if(res == ErrorInvalidBuffer)
            Scheduler::ThreadKillProcess("InvalidUserPointer");
        return res;
    }

    int64 Seek(uint64 descID, uint64 offs) {
        FileDescriptor* desc = (FileDescriptor*)descID;
        if(desc == nullptr)
            return ErrorInvalidFD;

        if(offs >= desc->node->infoFile.fileSize.Read())
            return ErrorSeekOffsetOOB;

        desc->pos.Write(offs);
        return OK;
    }
    SYSCALL_DEFINE2(syscall_seek, int64 desc, uint64 offs) {
        uint64 sysDesc;
        int64 error = Scheduler::ProcessGetSystemFileDescriptor(desc, sysDesc);
        if(error != OK) {
            return error;
        }

        return Seek(sysDesc, offs);
    }

}