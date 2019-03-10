#pragma once

#include "VFS.h"

namespace VFS
{
    class FloatingFileSystem : public FileSystem
    {
    public:
        void Mount(Node& mountPoint) override;
        void Unmount() override;

        void CreateNode(Node& folder, Node& node) override;
        void DestroyNode(Node& folder, Node& node) override;

        uint64 ReadFile(const Node& node, uint64 pos, void* buffer, uint64 bufferSize) override;
        void WriteFile(Node& node, uint64 pos, void* buffer, uint64 bufferSize) override;
    
        void ReadDirEntries(Node& folder) override;
    };
}