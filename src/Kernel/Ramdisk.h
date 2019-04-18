#pragma once

#include "VFS.h"
#include "RamdiskStructs.h"

class RamdiskFS : public VFS::FileSystem
{
public:
    RamdiskFS(const char* dev);
    ~RamdiskFS();

    void Mount(VFS::Node& mountPoint);
    void Unmount();

    void CreateNode(VFS::Node& folder, VFS::Node& node);
    void DestroyNode(VFS::Node& folder, VFS::Node& node);

    uint64 ReadNode(const VFS::Node& node, uint64 pos, void* buffer, uint64 bufferSize);
    void WriteNode(VFS::Node& node, uint64 pos, void* buffer, uint64 bufferSize);

    void ReadDirEntries(VFS::Node& folder);

private:
    uint64 m_DevFile;
    RamdiskHeader m_Header;
};