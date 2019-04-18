#include "VFS.h"

#include "ArrayList.h"
#include "string.h"
#include "conio.h"
#include "Device.h"
#include "FloatingFileSystem.h"

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

    static Node* AcquirePath(const char* path)
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
        Node* f = AcquirePath(folder);
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
        newNode->fs->CreateNode(*f, *newNode);

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
        Node* f = AcquirePath(folder);
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
        newNode->fs->CreateNode(*f, *newNode);

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
        Node* f = AcquirePath(folder);
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
        newNode->fs->CreateNode(*f, *newNode);

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

    void Mount(const char* mountPoint, FileSystem* fs)
    {
        Node* f = AcquirePath(mountPoint);
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
        Node* n = AcquirePath(path);
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
        } else {
            uint64 ret = n->fs->ReadNode(*n, pos, buffer, bufferSize);
            ReleaseNode(n);
            return ret;
        }
    }

    void WriteNode(uint64 node, uint64 pos, void* buffer, uint64 bufferSize)
    {
        Node* n = AcquireNode(node);

        if(n->type == Node::TYPE_DEVICE) {
            Device* dev = Device::GetByID(n->device.devID);
            ReleaseNode(n);
            dev->Write(pos, buffer, bufferSize);
            return;
        } else {
            n->fs->WriteNode(*n, pos, buffer, bufferSize);
            ReleaseNode(n);
            return;
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
            g_NodesLock.SpinLock();
            node->refCount = g_FirstFreeNode;
            g_FirstFreeNode = node->id;
            g_NodesLock.Unlock();
        }
        node->lock.Unlock();
    }

}