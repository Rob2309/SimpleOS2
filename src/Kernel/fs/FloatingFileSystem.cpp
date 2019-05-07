#include "FloatingFileSystem.h"

#include "stl/ArrayList.h"

namespace VFS {

    struct INode
    {
        char* data;
    };

    void FloatingFileSystem::Mount(Node& mountPoint)
    {
        mountPoint.fsNodeID = 0;
    }

    void FloatingFileSystem::Unmount()
    {

    }

    void FloatingFileSystem::CreateNode(Node& node)
    {
        if(node.type == Node::TYPE_FILE) {
            INode* n = new INode();
            n->data = nullptr;
            node.fsNodeID = (uint64)n;
        }
    }
    void FloatingFileSystem::DestroyNode(Node& node)
    {
        if(node.type == Node::TYPE_FILE) {
            INode* n = (INode*)node.fsNodeID;
            delete[] n->data;
            node.fsNodeID = 0;
        }
    }

    uint64 FloatingFileSystem::ReadNode(const Node& node, uint64 pos, void* buffer, uint64 bufferSize)
    {
        uint64 rem = node.file.size - pos;
        if(rem > bufferSize)
            rem = bufferSize;

        memcpy(buffer, ((INode*)node.fsNodeID)->data + pos, rem);
        return rem;
    }
    uint64 FloatingFileSystem::WriteNode(Node& node, uint64 pos, void* buffer, uint64 bufferSize)
    {
        INode* inode = (INode*)node.fsNodeID;
        uint64 neededSize = pos + bufferSize;

        if(neededSize > node.file.size) {
            char* n = new char[neededSize];
            memcpy(n, inode->data, node.file.size);
            delete[] inode->data;
            inode->data = n;
            node.file.size = neededSize;
        }

        memcpy(&inode->data[pos], buffer, bufferSize);
        return bufferSize;
    }

    void FloatingFileSystem::ReadDirEntries(Node& node)
    {
        // nop
    }

}