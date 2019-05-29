#pragma once

#include "FileSystem.h"

using namespace VFS;

class RamdiskFS : public VFS::FileSystem {
public:
    virtual void GetSuperBlock(SuperBlock* sb) override;
        
    virtual void CreateNode(Node* node) override;
    virtual void DestroyNode(Node* node) override;

    virtual void ReadNode(uint64 id, Node* node) override; 
    virtual void WriteNode(Node* node) override;

    virtual uint64 ReadNodeData(Node* node, uint64 pos, void* buffer, uint64 bufferSize) override;
    virtual uint64 WriteNodeData(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) override;

    virtual Directory* ReadDirEntries(Node* node) override;
    virtual Directory* WriteDirEntries(Node* node) override;

private:
    Node* CreateFolderNode();
};