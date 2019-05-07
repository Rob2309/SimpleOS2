#pragma once

#include "VFS.h"

namespace VFS
{
    /**
     * This file system is used for every file that does not belong to any other FileSystem
     **/
    class FloatingFileSystem : public FileSystem
    {
    public:
        void Mount(Node& mountPoint) override;
        void Unmount() override;

        void CreateNode(Node& node) override;
        void DestroyNode(Node& node) override;

        uint64 ReadNode(const Node& node, uint64 pos, void* buffer, uint64 bufferSize) override;
        uint64 WriteNode(Node& node, uint64 pos, void* buffer, uint64 bufferSize) override;
    
        void ReadDirEntries(Node& folder) override;
    };
}