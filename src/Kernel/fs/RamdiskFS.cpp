#include "RamdiskFS.h"

#include "terminal/conio.h"
#include "stdlib/string.h"

namespace VFS {

    RamdiskFS::RamdiskFS(const char* dev) {
        m_Dev = VFS::Open(dev);
        if(m_Dev == 0)
            printf("Failed to open %s\n", dev);

        VFS::Read(m_Dev, &m_Header, sizeof(m_Header));

        m_Files = new RamdiskFile[m_Header.numFiles];
        VFS::Read(m_Dev, m_Files, m_Header.numFiles * sizeof(RamdiskFile));
    }

    void RamdiskFS::GetSuperBlock(SuperBlock* sb) {
        sb->rootNode = (uint64)-1;
    }

    void RamdiskFS::CreateNode(Node* node) {
    }
    void RamdiskFS::DestroyNode(Node* node) {
    }

    void RamdiskFS::ReadNode(uint64 id, Node* node) {
        if(id == (uint64)-1) {
            node->id = id;
            node->fs = this;
            node->linkRefCount = 1;
            node->type = Node::TYPE_DIRECTORY;
            node->dir = Directory::Create(m_Header.numFiles);

            for(int i = 0; i < m_Header.numFiles; i++) {
                DirectoryEntry* entry;
                Directory::AddEntry(&node->dir, &entry);

                strcpy(entry->name, m_Files[i].name);
                entry->nodeID = i;
            }
        } else {
            node->id = id;
            node->fs = this;
            node->linkRefCount = 1;
            node->type = Node::TYPE_FILE;
            node->fileSize = m_Files[id].size;
        }
    }
    void RamdiskFS::WriteNode(Node* node) {
    }

    uint64 RamdiskFS::ReadNodeData(Node* node, uint64 pos, void* buffer, uint64 bufferSize) {
        RamdiskFile* file = &m_Files[node->id];

        uint64 rem = file->size - pos;
        if(rem > bufferSize)
            rem = bufferSize;

        return VFS::Read(m_Dev, file->dataOffset + pos, buffer, rem);
    }
    uint64 RamdiskFS::WriteNodeData(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) {
        return 0;
    }

    Directory* RamdiskFS::ReadDirEntries(Node* node) {
    }
    Directory* RamdiskFS::WriteDirEntries(Node* node) {
    }

}