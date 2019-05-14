#pragma once

#include "types.h"
#include "VFS.h"
#include "Directory.h"

namespace VFS {

    struct Node;

    struct SuperBlock {
        uint64 rootNode;
    };

    class FileSystem
    {
    public:
        virtual void GetSuperBlock(SuperBlock* sb) = 0;
        
        virtual void CreateNode(Node* node) = 0;
        virtual void DestroyNode(Node* node) = 0;

        virtual void ReadNode(uint64 id, Node* node) = 0; 
        virtual void WriteNode(Node* node) = 0;

        virtual uint64 ReadNodeData(Node* node, uint64 pos, void* buffer, uint64 bufferSize) = 0;
        virtual uint64 WriteNodeData(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) = 0;
    
        virtual Directory* ReadDirEntries(Node* node) = 0;
        virtual Directory* WriteDirEntries(Node* node) = 0;
    };

}