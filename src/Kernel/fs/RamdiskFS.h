#pragma once

#include "FileSystem.h"
#include "RamdiskStructs.h"

namespace VFS {
    class RamdiskFS : public FileSystem {
    public:
        RamdiskFS(const char* dev);

        void GetSuperBlock(SuperBlock* sb) override;

        void CreateNode(Node* node) override;
        void DestroyNode(Node* node) override;

        void ReadNode(uint64 id, Node* node) override; 
        void WriteNode(Node* node) override;

        uint64 ReadNodeData(Node* node, uint64 pos, void* buffer, uint64 bufferSize) override;
        uint64 WriteNodeData(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) override;

        Directory* ReadDirEntries(Node* node) override;
        Directory* WriteDirEntries(Node* node) override;

        uint64 GetNodeSize(Node* node) override;

    private:
        uint64 m_Dev;
        RamdiskFile* m_Files;
        RamdiskHeader m_Header;
    };
}