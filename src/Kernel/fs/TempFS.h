#pragma once

#include "FileSystem.h"

class TempFS : public VFS::FileSystem {
public:
    TempFS();

    void GetSuperBlock(VFS::SuperBlock* sb) override;
    void SetMountPoint(VFS::MountPoint* mp) override;
    void PrepareUnmount() override;

    void CreateNode(VFS::Node* node) override;
    void DestroyNode(VFS::Node* node) override;

    void UpdateDir(VFS::Node* node) override;
    
    void ReadNode(uint64 id, VFS::Node* node) override;
    void WriteNode(VFS::Node* node) override;

    virtual uint64 ReadNodeData(VFS::Node* node, uint64 pos, void* buffer, uint64 bufferSize) override;
    virtual uint64 WriteNodeData(VFS::Node* node, uint64 pos, const void* buffer, uint64 bufferSize) override;
    virtual void ClearNodeData(VFS::Node* node) override;

private:
    uint64 m_RootNodeID;
    VFS::MountPoint* m_MP;
};