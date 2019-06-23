#pragma once

#include "FileSystem.h"
#include "Mutex.h"
#include "ktl/vector.h"

namespace VFS {

    struct Pipe {
        uint64 id;
        bool free;
        Mutex lock;
        uint64 readPos;
        uint64 writePos;
        char buffer[4096];
    };

    class PipeFS : public FileSystem {
    public:
        void GetSuperBlock(SuperBlock* sb) override;

        void CreateNode(Node* node) override;
        void DestroyNode(Node* node) override;

        void ReadNode(uint64 id, Node* node) override;
        void WriteNode(Node* node) override;

        uint64 ReadNodeData(Node* node, uint64 pos, void* buffer, uint64 bufferSize) override;
        uint64 WriteNodeData(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) override;

        Directory* ReadDirEntries(Node* node) override;
        Directory* WriteDirEntries(Node* node) override;

    private:
        Mutex m_PipesLock;
        ktl::vector<Pipe*> m_Pipes;
    };

}