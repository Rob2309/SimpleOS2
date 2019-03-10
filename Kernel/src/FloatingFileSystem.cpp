#include "FloatingFileSystem.h"

#include "ArrayList.h"

namespace VFS {

    struct INode
    {
        char* data;
    };

    void FloatingFileSystem::Mount(Node& mountPoint)
    {
        mountPoint.fsNode = 0;
    }

    void FloatingFileSystem::Unmount()
    {

    }

    void FloatingFileSystem::CreateNode(Node& folder, Node& node)
    {
        if(node.type == Node::TYPE_FILE) {
            INode* n = new INode();
            n->data = nullptr;
            node.fsNode = (uint64)n;
        }
    }
    void FloatingFileSystem::DestroyNode(Node& folder, Node& node)
    {
        if(node.type == Node::TYPE_FILE) {
            INode* n = new INode();
            delete[] n->data;
        }
    }

    uint64 FloatingFileSystem::ReadFile(const Node& node, uint64 pos, void* buffer, uint64 bufferSize)
    {
        uint64 rem = node.file.size - pos;
        if(rem > bufferSize)
            rem = bufferSize;

        memcpy(buffer, ((INode*)node.fsNode)->data, rem);
        return rem;
    }
    void FloatingFileSystem::WriteFile(Node& node, uint64 pos, void* buffer, uint64 bufferSize)
    {
        INode* inode = (INode*)node.fsNode;
        uint64 neededSize = pos + bufferSize;

        if(neededSize > node.file.size) {
            char* n = new char[neededSize];
            memcpy(n, inode->data, node.file.size);
            delete[] inode->data;
            inode->data = n;
            node.file.size = neededSize;
        }

        memcpy(&inode->data[pos], buffer, bufferSize);
    }

    void FloatingFileSystem::ReadDirEntries(Node& node)
    {
        // nop
    }

}