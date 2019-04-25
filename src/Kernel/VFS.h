#pragma once

#include "types.h"
#include "Mutex.h"

namespace VFS {

    constexpr uint64 PipeBufferSize = 4096;

    class FileSystem;

    struct Node
    {
        enum Type {
            TYPE_FILE,
            TYPE_DIRECTORY,
            TYPE_DEVICE,

            TYPE_PIPE,
        } type;

        union {
            struct {
                uint64 size;
            } file;
            struct {
                uint64 numFiles;
                uint64* files;
            } directory;
            struct {
                uint64 devID;
            } device;
            struct {
                uint64 head;
                uint64 tail;
                char* buffer;
            } pipe;
        };

        char name[50];

        FileSystem* fs;
        uint64 fsNodeID;

        uint64 id;
        uint64 refCount;

        Mutex lock;
    };

    class FileSystem
    {
    public:
        virtual void Mount(Node& mountPoint) = 0;
        virtual void Unmount() = 0;

        virtual void CreateNode(Node& node) = 0;
        virtual void DestroyNode(Node& node) = 0;

        virtual uint64 ReadNode(const Node& node, uint64 pos, void* buffer, uint64 bufferSize) = 0;
        virtual uint64 WriteNode(Node& node, uint64 pos, void* buffer, uint64 bufferSize) = 0;
    
        virtual void ReadDirEntries(Node& folder) = 0;
    };

    void Init();
    
    bool CreateFile(const char* folder, const char* name);
    bool CreateFolder(const char* folder, const char* name);
    bool CreateDeviceFile(const char* folder, const char* name, uint64 devID);
    uint64 CreatePipe();
    bool DeleteFile(const char* file);

    void Mount(const char* mountPoint, FileSystem* fs);

    uint64 OpenNode(const char* path);
    uint64 CloseNode(uint64 node);

    uint64 GetSize(uint64 node);
    uint64 ReadNode(uint64 node, uint64 pos, void* buffer, uint64 bufferSize);
    uint64 WriteNode(uint64 node, uint64 pos, void* buffer, uint64 bufferSize);

    Node* AcquireNode(uint64 id);
    Node* CreateNode();
    void ReleaseNode(Node* node);

}