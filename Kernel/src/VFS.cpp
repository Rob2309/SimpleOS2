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

        Node* node;
        uint64 pos;
    };

    static Node g_RootNode;
    static ArrayList<FileDescriptor>* g_FileDescriptors;
    static uint64 g_FileDescCounter = 1;

    void Init()
    {
        g_FileDescriptors = new ArrayList<FileDescriptor>();

        g_RootNode.type = Node::TYPE_DIRECTORY;
        g_RootNode.directory.numFiles = 0;
        g_RootNode.directory.files = nullptr;
        g_RootNode.fs = new FloatingFileSystem();
        g_RootNode.numReaders = 0;
        g_RootNode.numWriters = 0;

        g_RootNode.fs->Mount(g_RootNode);
    }

    static Node* FindNode(Node* folder, const char* name)
    {
        if(folder->type != Node::TYPE_DIRECTORY)
            return nullptr;

        for(int i = 0; i < folder->directory.numFiles; i++) {
            if(strcmp(name, folder->directory.files[i].name) == 0)
                return &folder->directory.files[i];
        }

        return nullptr;
    }

    static Node* FindPath(const char* path)
    {
        static char nameBuffer[50] = { 0 };

        uint64 pathPos = 1;
        Node* node = &g_RootNode;

        if(path[pathPos] == '\0')
            return &g_RootNode;

        uint64 bufferPos = 0;

        while(true) {
            char c = path[pathPos];
            pathPos++;
            if(c == '/') {
                nameBuffer[bufferPos] = '\0';

                node = FindNode(node, nameBuffer);
                if(node == nullptr)
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

        Node newNode;
        newNode.type = Node::TYPE_FILE;
        newNode.file.size = 0;
        memcpy(newNode.name, name, strlen(name) + 1);
        newNode.fs = f->fs;
        newNode.numReaders = 0;
        newNode.numWriters = 0;
        newNode.fs->CreateNode(*f, newNode);

        Node* files = new Node[f->directory.numFiles + 1];
        memcpy(files, f->directory.files, f->directory.numFiles * sizeof(Node));
        files[f->directory.numFiles] = newNode;
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

        Node newNode;
        newNode.type = Node::TYPE_DIRECTORY;
        newNode.directory.numFiles = 0;
        newNode.directory.files = nullptr;
        memcpy(newNode.name, name, strlen(name) + 1);
        newNode.fs = f->fs;
        newNode.numReaders = 0;
        newNode.numWriters = 0;
        newNode.fs->CreateNode(*f, newNode);

        Node* files = new Node[f->directory.numFiles + 1];
        memcpy(files, f->directory.files, f->directory.numFiles * sizeof(Node));
        files[f->directory.numFiles] = newNode;
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

        Node newNode;
        newNode.type = Node::TYPE_DEVICE;
        newNode.device.devID = devID;
        memcpy(newNode.name, name, strlen(name) + 1);
        newNode.fs = f->fs;
        newNode.numReaders = 0;
        newNode.numWriters = 0;
        newNode.fs->CreateNode(*f, newNode);

        Node* files = new Node[f->directory.numFiles + 1];
        memcpy(files, f->directory.files, f->directory.numFiles * sizeof(Node));
        files[f->directory.numFiles] = newNode;
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
        desc.node = n;
        desc.pos = 0;

        g_FileDescriptors->push_back(desc);

        return desc.id;
    }
    void CloseFile(uint64 file) 
    {
        for(auto a = g_FileDescriptors->begin(); a != g_FileDescriptors->end(); ++a)
        {
            if(a->id == file) {
                g_FileDescriptors->erase(a);
                return;
            }
        }
    }

    uint64 GetFileSize(uint64 file)
    {
        for(FileDescriptor& desc : *g_FileDescriptors) {
            if(desc.id == file) {
                return desc.node->file.size;
            }
        }
    }

    uint64 ReadFile(uint64 file, void* buffer, uint64 bufferSize)
    {
        for(FileDescriptor& desc : *g_FileDescriptors) {
            if(desc.id == file) {
                if(desc.node->type == Node::TYPE_DEVICE) {
                    Device* dev = Device::GetByID(desc.node->device.devID);
                    uint64 ret = dev->Read(desc.pos, buffer, bufferSize);
                    desc.pos += ret;
                    return ret;
                } else {
                    uint64 ret = desc.node->fs->ReadFile(*desc.node, desc.pos, buffer, bufferSize);
                    desc.pos += ret;
                    return ret;
                }
            }
        }

        return 0;
    }

    void WriteFile(uint64 file, void* buffer, uint64 bufferSize)
    {
        for(FileDescriptor& desc : *g_FileDescriptors) {
            if(desc.id == file) {
                if(desc.node->type == Node::TYPE_DEVICE) {
                    Device* dev = Device::GetByID(desc.node->device.devID);
                    dev->Write(desc.pos, buffer, bufferSize);
                    desc.pos += bufferSize;
                    return;
                } else {
                    desc.node->fs->WriteFile(*desc.node, desc.pos, buffer, bufferSize);
                    desc.pos += bufferSize;
                    return;
                }
            }
        }
    }

    void SeekFile(uint64 file, uint64 pos)
    {
        for(FileDescriptor& desc : *g_FileDescriptors) {
            if(desc.id == file) {
                desc.pos = pos;
            }
        }
    }

}