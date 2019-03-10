#pragma once

#include "types.h"

namespace VFS {

    class FileSystem;

    struct Node
    {
        enum Type {
            TYPE_FILE,
            TYPE_DIRECTORY,
            TYPE_DEVICE,
        } type;

        union {
            struct {
                uint64 size;
            } file;
            struct {
                uint64 numFiles;
                Node* files;
            } directory;
            struct {
                uint64 devID;
            } device;
        };

        char name[50];

        FileSystem* fs;
        uint64 fsNode;

        uint64 numReaders;
        uint64 numWriters;
    };

    class FileSystem
    {
    public:
        virtual void Mount(Node& mountPoint) = 0;
        virtual void Unmount() = 0;

        virtual void CreateNode(Node& folder, Node& node) = 0;
        virtual void DestroyNode(Node& folder, Node& node) = 0;

        virtual uint64 ReadFile(const Node& node, uint64 pos, void* buffer, uint64 bufferSize) = 0;
        virtual void WriteFile(Node& node, uint64 pos, void* buffer, uint64 bufferSize) = 0;
    
        virtual void ReadDirEntries(Node& folder) = 0;
    };

    void Init();

    void Mount(const char* mountPoint, FileSystem* fs);
    
    bool CreateFile(const char* folder, const char* name);
    bool CreateFolder(const char* folder, const char* name);
    bool CreateDeviceFile(const char* folder, const char* name, uint64 devID);
    bool DeleteFile(const char* file);

    uint64 OpenFile(const char* path);
    void CloseFile(uint64 file);

    uint64 GetFileSize(uint64 file);

    uint64 ReadFile(uint64 file, void* buffer, uint64 bufferSize);
    void WriteFile(uint64 file, void* buffer, uint64 bufferSize);
    void SeekFile(uint64 file, uint64 pos);

}