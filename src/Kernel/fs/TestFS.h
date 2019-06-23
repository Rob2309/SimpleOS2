#pragma once

#include "FileSystem.h"

class TestFS : public VFS::FileSystem {
public:
    TestFS();

    void GetSuperBlock(VFS::SuperBlock* sb) override;

    void CreateNode(VFS::Node* node) override;
    void DestroyNode(VFS::Node* node) override;
    
    void ReadNode(uint64 id, VFS::Node* node) override;
    void WriteNode(VFS::Node* node) override;

    virtual uint64 ReadNodeData(VFS::Node* node, uint64 pos, void* buffer, uint64 bufferSize) override;
    virtual uint64 WriteNodeData(VFS::Node* node, uint64 pos, const void* buffer, uint64 bufferSize) override;

    virtual VFS::Directory* ReadDirEntries(VFS::Node* node) override;
    virtual void WriteDirEntries(VFS::Node* node) override;

private:
    uint64 m_RootNodeID;
};