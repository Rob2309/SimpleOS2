#include "VFS.h"

#include "ArrayList.h"
#include "string.h"
#include "conio.h"
#include "Device.h"
#include "FloatingFileSystem.h"

namespace VFS {

    struct FileDescriptor
    {
        FileSystem* fs;
        uint64 fsNode;

        uint64 pos;
    };

    static Node g_RootNode;

    void Init()
    {
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

}