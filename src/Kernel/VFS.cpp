#include "VFS.h"

#include "ArrayList.h"
#include "string.h"
#include "conio.h"
#include "Device.h"
#include "FloatingFileSystem.h"
#include "Scheduler.h"

namespace VFS {

    static Mutex g_NodesLock;
    static ArrayList<Node*> g_Nodes;
    static uint64 g_FirstFreeNode;

    void Init()
    {
        Node* root = new Node();
        root->id = 1;
        root->type = Node::TYPE_DIRECTORY;
        root->directory.numFiles = 0;
        root->directory.files = nullptr;
        root->fs = new FloatingFileSystem();
        root->refCount = 1;
        root->fs->Mount(*root);
        root->lock.Unlock();
        g_Nodes.push_back(root);

        g_FirstFreeNode = 0;
    }

    static Node* FindFolderEntry(Node* folder, const char* name)
    {
        if(folder->type != Node::TYPE_DIRECTORY)
            return nullptr;

        for(int i = 0; i < folder->directory.numFiles; i++) {
            Node* n = AcquireNode(folder->directory.files[i]);
            if(strcmp(name, n->name) == 0)
                return n;
            ReleaseNode(n);
        }

        return nullptr;
    }

    static Node* AcquirePath(const char* path, Node** containingFolder)
    {
        static char nameBuffer[50] = { 0 };

        uint64 pathPos = 1;
        Node* node = AcquireNode(1);

        if(path[pathPos] == '\0')
            return node;

        uint64 bufferPos = 0;

        while(true) {
            char c = path[pathPos];
            pathPos++;
            if(c == '/') {
                nameBuffer[bufferPos] = '\0';

                Node* nextNode = FindFolderEntry(node, nameBuffer);
                ReleaseNode(node);
                node = nextNode;
                if(node == nullptr)
                    return nullptr;
                if(node->type != Node::TYPE_DIRECTORY) {
                    ReleaseNode(node);
                    return nullptr;
                }

                bufferPos = 0;
            } else if(c == '\0') {
                nameBuffer[bufferPos] = '\0';
                Node* res = FindFolderEntry(node, nameBuffer);
                if(res != nullptr && containingFolder != nullptr)
                    *containingFolder = node;
                else
                    ReleaseNode(node);
                return res;
            } else {
                nameBuffer[bufferPos] = c;
                bufferPos++;
            }
        }
    }

    bool CreateFile(const char* folder, const char* name) 
    {
        Node* f = AcquirePath(folder, nullptr);
        if(f == nullptr)
            return false;
        if(f->type != Node::TYPE_DIRECTORY) {
            ReleaseNode(f);
            return false;
        }

        Node* newNode = CreateNode();
        newNode->type = Node::TYPE_FILE;
        newNode->file.size = 0;
        memcpy(newNode->name, name, strlen(name) + 1);
        newNode->fs = f->fs;
        newNode->fs->CreateNode(*newNode);

        newNode->refCount++; // ref of folder entry

        uint64* files = new uint64[f->directory.numFiles + 1];
        memcpy(files, f->directory.files, f->directory.numFiles * sizeof(uint64));
        files[f->directory.numFiles] = newNode->id;
        delete[] f->directory.files;
        f->directory.files = files;
        f->directory.numFiles++;

        ReleaseNode(newNode);
        ReleaseNode(f);

        return true;
    }

    bool CreateFolder(const char* folder, const char* name) 
    {
        Node* f = AcquirePath(folder, nullptr);
        if(f == nullptr)
            return false;
        if(f->type != Node::TYPE_DIRECTORY) {
            ReleaseNode(f);
            return false;
        }

        Node* newNode = CreateNode();
        newNode->type = Node::TYPE_DIRECTORY;
        newNode->directory.numFiles = 0;
        newNode->directory.files = nullptr;
        memcpy(newNode->name, name, strlen(name) + 1);
        newNode->fs = f->fs;
        newNode->fs->CreateNode(*newNode);

        newNode->refCount++;

        uint64* files = new uint64[f->directory.numFiles + 1];
        memcpy(files, f->directory.files, f->directory.numFiles * sizeof(uint64));
        files[f->directory.numFiles] = newNode->id;
        delete[] f->directory.files;
        f->directory.files = files;
        f->directory.numFiles++;

        ReleaseNode(newNode);
        ReleaseNode(f);
        
        return true;
    }

    bool CreateDeviceFile(const char* folder, const char* name, uint64 devID) 
    {
        Node* f = AcquirePath(folder, nullptr);
        if(f == nullptr)
            return false;
        if(f->type != Node::TYPE_DIRECTORY) {
            ReleaseNode(f);
            return false;
        }

        Node* newNode = CreateNode();
        newNode->type = Node::TYPE_DEVICE;
        newNode->device.devID = devID;
        memcpy(newNode->name, name, strlen(name) + 1);
        newNode->fs = f->fs;
        newNode->fs->CreateNode(*newNode);

        newNode->refCount++;

        uint64* files = new uint64[f->directory.numFiles + 1];
        memcpy(files, f->directory.files, f->directory.numFiles * sizeof(uint64));
        files[f->directory.numFiles] = newNode->id;
        delete[] f->directory.files;
        f->directory.files = files;
        f->directory.numFiles++;

        ReleaseNode(newNode);
        ReleaseNode(f);

        return true;
    }

    uint64 CreatePipe()
    {
        Node* node = CreateNode();

        node->type = Node::TYPE_PIPE;
        node->pipe.head = 0;
        node->pipe.tail = 0;
        node->pipe.buffer = new char[PipeBufferSize];

        node->fs = nullptr;
        
        node->refCount += 2;

        uint64 ret = node->id;
        ReleaseNode(node);
        return ret;
    }

    bool DeleteFile(const char* file) 
    {
        Node* folder;
        Node* node = AcquirePath(file, &folder);
        if(node == nullptr)
            return false;

        uint64* files = new uint64[folder->directory.numFiles - 1];
        int index = 0;
        for(int i = 0; i < folder->directory.numFiles; i++) {
            uint64 f = folder->directory.files[i];
            if(f != node->id) {
                files[index++] = f;
            }
        }

        delete[] folder->directory.files;
        folder->directory.numFiles--;
        folder->directory.files = files;

        node->refCount--;

        ReleaseNode(folder);
        ReleaseNode(node);

        return true;
    }

    void Mount(const char* mountPoint, FileSystem* fs)
    {
        Node* f = AcquirePath(mountPoint, nullptr);
        if(f == nullptr)
            return;
        if(f->type != Node::TYPE_DIRECTORY || f->directory.numFiles != 0) {
            ReleaseNode(f);
            return;
        }

        f->fs = fs;
        f->fs->Mount(*f);
        ReleaseNode(f);
    }

    uint64 OpenNode(const char* path)
    {
        Node* n = AcquirePath(path, nullptr);
        if(n == nullptr)
            return 0;
        if(n->type == Node::TYPE_DIRECTORY) {
            ReleaseNode(n);
            return 0;
        }

        n->refCount++;
        uint64 res = n->id;
        ReleaseNode(n);
        return res;
    }
    uint64 CloseNode(uint64 node)
    {
        Node* n = AcquireNode(node);
        n->refCount--;
        ReleaseNode(n);
    }

    uint64 GetSize(uint64 node)
    {
        Node* n = AcquireNode(node);
        uint64 res = n->file.size;
        ReleaseNode(n);
        return res;
    }

    uint64 ReadNode(uint64 node, uint64 pos, void* buffer, uint64 bufferSize)
    {
        Node* n = AcquireNode(node);

        if(n->type == Node::TYPE_DEVICE) {
            Device* dev = Device::GetByID(n->device.devID);
            ReleaseNode(n);
            uint64 ret = dev->Read(pos, buffer, bufferSize);
            return ret;
        } else if(n->type == Node::TYPE_PIPE) {
            uint64 end = n->pipe.tail;
            if(n->pipe.tail < n->pipe.head)
                n->pipe.tail += PipeBufferSize;
            uint64 size = end - n->pipe.head;
            if(size > bufferSize)
                size = bufferSize;
            for(int i = 0; i < size; i++) {
                ((char*)buffer)[i] = n->pipe.buffer[(n->pipe.head + i) % PipeBufferSize];
            }
            n->pipe.head += size;
            n->pipe.head %= PipeBufferSize;
            
            if(size > 0)
                Scheduler::NotifyNodeRead(n->id);

            ReleaseNode(n);
            return size;
        } else {
            uint64 ret = n->fs->ReadNode(*n, pos, buffer, bufferSize);
            ReleaseNode(n);
            return ret;
        }
    }

    uint64 WriteNode(uint64 node, uint64 pos, void* buffer, uint64 bufferSize)
    {
        Node* n = AcquireNode(node);

        if(n->type == Node::TYPE_DEVICE) {
            Device* dev = Device::GetByID(n->device.devID);
            ReleaseNode(n);
            return dev->Write(pos, buffer, bufferSize);
        } else if(n->type == Node::TYPE_PIPE) {
            uint64 end = n->pipe.tail;
            if(n->pipe.tail < n->pipe.head)
                n->pipe.tail += PipeBufferSize;
            uint64 size = PipeBufferSize - (end - n->pipe.head);
            if(size > bufferSize)
                size = bufferSize;
            for(int i = 0; i < size; i++) {
                n->pipe.buffer[(n->pipe.tail + i) % PipeBufferSize] = ((char*)buffer)[i];
            }
            n->pipe.tail += size;
            n->pipe.tail %= PipeBufferSize;

            if(size > 0)
                Scheduler::NotifyNodeWrite(n->id);

            ReleaseNode(n);
            return size;
        } else {
            uint64 res = n->fs->WriteNode(*n, pos, buffer, bufferSize);
            ReleaseNode(n);
            return res;
        }
    }

    Node* AcquireNode(uint64 id) {
        g_NodesLock.SpinLock();
        Node* res = g_Nodes[id - 1];
        res->lock.SpinLock();
        res->refCount++;
        g_NodesLock.Unlock();
        return res;
    }

    Node* CreateNode()
    {
        g_NodesLock.SpinLock();
        if(g_FirstFreeNode != 0) {
            Node* ret = g_Nodes[g_FirstFreeNode - 1];
            ret->lock.SpinLock();
            g_FirstFreeNode = ret->refCount;
            ret->refCount = 1;
            g_NodesLock.Unlock();
            return ret;
        } else {
            Node* n = new Node();
            n->id = g_Nodes.size() + 1;
            n->refCount = 1;
            n->lock.SpinLock();
            g_Nodes.push_back(n);
            g_NodesLock.Unlock();
            return n;
        }
    }

    void ReleaseNode(Node* node) {
        node->refCount--;
        if(node->refCount == 0) {
            if(node->fs != nullptr)
                node->fs->DestroyNode(*node);
            g_NodesLock.SpinLock();
            node->refCount = g_FirstFreeNode;
            g_FirstFreeNode = node->id;
            g_NodesLock.Unlock();
        }
        node->lock.Unlock();
    }

}