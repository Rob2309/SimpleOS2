#include "VFS.h"

#include "ArrayList.h"
#include "string.h"
#include "conio.h"
#include "Device.h"
#include "FloatingFileSystem.h"

namespace VFS {

    struct FileDescriptor
    {
        uint64 id;

        uint64 node;
        uint64 pos;
    };

    static ArrayList<Node*> g_Nodes;
    static uint64 g_FirstFreeNode;

    static ArrayList<FileDescriptor> g_FileDescriptors;
    static uint64 g_FileDescCounter = 1;

    void Init()
    {
        Node* root = new Node();
        root->id = 1;
        root->type = Node::TYPE_DIRECTORY;
        root->directory.numFiles = 0;
        root->directory.files = nullptr;
        root->fs = new FloatingFileSystem();
        root->numReaders = 0;
        root->numWriters = 0;
        root->fs->Mount(*root);
        g_Nodes.push_back(root);

        g_FirstFreeNode = 0;
    }

    static Node* FindNode(Node* folder, const char* name)
    {
        if(folder->type != Node::TYPE_DIRECTORY)
            return nullptr;

        for(int i = 0; i < folder->directory.numFiles; i++) {
            Node* n = GetNode(folder->directory.files[i]);
            if(strcmp(name, n->name) == 0)
                return n;
        }

        return nullptr;
    }

    static Node* FindPath(const char* path)
    {
        static char nameBuffer[50] = { 0 };

        uint64 pathPos = 1;
        Node* node = GetNode(1);

        if(path[pathPos] == '\0')
            return node;

        uint64 bufferPos = 0;

        while(true) {
            char c = path[pathPos];
            pathPos++;
            if(c == '/') {
                nameBuffer[bufferPos] = '\0';

                node = FindNode(node, nameBuffer);
                if(&node == nullptr)
                    return nullptr;
                if(node->type != Node::TYPE_DIRECTORY)
                    return nullptr;

                bufferPos = 0;
            } else if(c == '\0') {
                nameBuffer[bufferPos] = '\0';
                return FindNode(node, nameBuffer);
            } else {
                nameBuffer[bufferPos] = c;
                bufferPos++;
            }
        }
    }

    void Mount(const char* mountPoint, FileSystem* fs)
    {
        Node* f = FindPath(mountPoint);
        if(f == nullptr || f->type != Node::TYPE_DIRECTORY)
            return;
        if(f->directory.numFiles != 0)
            return;

        f->fs = fs;
        f->fs->Mount(*f);
    }

    bool CreateFile(const char* folder, const char* name) 
    {
        Node* f = FindPath(folder);
        if(f == nullptr || f->type != Node::TYPE_DIRECTORY)
            return false;

        Node* newNode = GetFreeNode();
        newNode->type = Node::TYPE_FILE;
        newNode->file.size = 0;
        memcpy(newNode->name, name, strlen(name) + 1);
        newNode->fs = f->fs;
        newNode->numReaders = 0;
        newNode->numWriters = 0;
        newNode->fs->CreateNode(*f, *newNode);

        uint64* files = new uint64[f->directory.numFiles + 1];
        memcpy(files, f->directory.files, f->directory.numFiles * sizeof(uint64));
        files[f->directory.numFiles] = newNode->id;
        delete[] f->directory.files;
        f->directory.files = files;
        f->directory.numFiles++;

        return true;
    }

    bool CreateFolder(const char* folder, const char* name) 
    {
        Node* f = FindPath(folder);
        if(f == nullptr || f->type != Node::TYPE_DIRECTORY)
            return false;

        Node* newNode = GetFreeNode();
        newNode->type = Node::TYPE_DIRECTORY;
        newNode->directory.numFiles = 0;
        newNode->directory.files = nullptr;
        memcpy(newNode->name, name, strlen(name) + 1);
        newNode->fs = f->fs;
        newNode->numReaders = 0;
        newNode->numWriters = 0;
        newNode->fs->CreateNode(*f, *newNode);

        uint64* files = new uint64[f->directory.numFiles + 1];
        memcpy(files, f->directory.files, f->directory.numFiles * sizeof(uint64));
        files[f->directory.numFiles] = newNode->id;
        delete[] f->directory.files;
        f->directory.files = files;
        f->directory.numFiles++;
        
        return true;
    }

    bool CreateDeviceFile(const char* folder, const char* name, uint64 devID) 
    {
        Node* f = FindPath(folder);
        if(f == nullptr || f->type != Node::TYPE_DIRECTORY)
            return false;

        Node* newNode = GetFreeNode();
        newNode->type = Node::TYPE_DEVICE;
        newNode->device.devID = devID;
        memcpy(newNode->name, name, strlen(name) + 1);
        newNode->fs = f->fs;
        newNode->numReaders = 0;
        newNode->numWriters = 0;
        newNode->fs->CreateNode(*f, *newNode);

        uint64* files = new uint64[f->directory.numFiles + 1];
        memcpy(files, f->directory.files, f->directory.numFiles * sizeof(uint64));
        files[f->directory.numFiles] = newNode->id;
        delete[] f->directory.files;
        f->directory.files = files;
        f->directory.numFiles++;

        return true;
    }

    uint64 OpenFile(const char* path)
    {
        Node* n = FindPath(path);
        if(n == nullptr || n->type == Node::TYPE_DIRECTORY)
            return 0;

        FileDescriptor desc;
        desc.id = g_FileDescCounter++;
        desc.node = n->id;
        desc.pos = 0;

        g_FileDescriptors.push_back(desc);

        return desc.id;
    }
    void CloseFile(uint64 file) 
    {
        for(auto a = g_FileDescriptors.begin(); a != g_FileDescriptors.end(); ++a)
        {
            if(a->id == file) {
                g_FileDescriptors.erase(a);
                return;
            }
        }
    }

    uint64 GetFileSize(uint64 file)
    {
        for(FileDescriptor& desc : g_FileDescriptors) {
            if(desc.id == file) {
                return GetNode(desc.node)->file.size;
            }
        }
    }

    uint64 ReadFile(uint64 file, void* buffer, uint64 bufferSize)
    {
        for(FileDescriptor& desc : g_FileDescriptors) {
            if(desc.id == file) {
                Node* n = GetNode(desc.node);
                if(n->type == Node::TYPE_DEVICE) {
                    Device* dev = Device::GetByID(n->device.devID);
                    uint64 ret = dev->Read(desc.pos, buffer, bufferSize);
                    desc.pos += ret;
                    return ret;
                } else {
                    uint64 ret = n->fs->ReadFile(*n, desc.pos, buffer, bufferSize);
                    desc.pos += ret;
                    return ret;
                }
            }
        }

        return 0;
    }

    void WriteFile(uint64 file, void* buffer, uint64 bufferSize)
    {
        for(FileDescriptor& desc : g_FileDescriptors) {
            if(desc.id == file) {
                Node* n = GetNode(desc.node);
                if(n->type == Node::TYPE_DEVICE) {
                    Device* dev = Device::GetByID(n->device.devID);
                    dev->Write(desc.pos, buffer, bufferSize);
                    desc.pos += bufferSize;
                    return;
                } else {
                    n->fs->WriteFile(*n, desc.pos, buffer, bufferSize);
                    desc.pos += bufferSize;
                    return;
                }
            }
        }
    }

    void SeekFile(uint64 file, uint64 pos)
    {
        for(FileDescriptor& desc : g_FileDescriptors) {
            if(desc.id == file) {
                desc.pos = pos;
            }
        }
    }

    Node* GetNode(uint64 id) {
        return g_Nodes[id - 1];
    }

    Node* GetFreeNode()
    {
        if(g_FirstFreeNode != 0) {
            Node* ret = GetNode(g_FirstFreeNode);
            g_FirstFreeNode = ret->numReaders;
            return ret;
        } else {
            Node* n = new Node();
            memset(n, 0, sizeof(Node));
            n->id = g_Nodes.size() + 1;
            g_Nodes.push_back(n);
            return n;
        }
    }

}