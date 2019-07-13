#include "ext2.h"

#include "klib/stdio.h"
#include "klib/string.h"

using namespace VFS;

namespace Ext2 {

    Ext2Driver::Ext2Driver(BlockDeviceDriver* driver, uint64 subID) {
        m_Driver = driver;
        m_Dev = subID;

        m_Driver->GetData(m_Dev, 1024, &m_SB, sizeof(SuperBlock));

        klog_info("Ext2", "Ramdisk has version %i.%i", m_SB.versionMajor, m_SB.versionMinor);
        klog_info("Ext2", "Volume name: %s", m_SB.volumeName);
    }

    void Ext2Driver::GetSuperBlock(VFS::SuperBlock* sb) {
        sb->rootNode = 2;
    }

    void Ext2Driver::CreateNode(Node* node) {
    }
    void Ext2Driver::DestroyNode(Node* node) {
    }

    void Ext2Driver::ReadNode(uint64 id, Node* node) {
        node->id = id;
        node->fs = this;

        uint64 group = (id - 1) / m_SB.inodesPerGroup;
        
        BlockGroupDesc desc;
        m_Driver->GetData(m_Dev, group * 32 + 2048, &desc, sizeof(BlockGroupDesc));

        uint64 relID = (id - 1) % m_SB.inodesPerGroup;

        uint64 inodePos = desc.inodeTableBlock * (1024 << m_SB.blockSizeShift) + relID * m_SB.inodeSize;
        INode* inode = new INode();
        m_Driver->GetData(m_Dev, inodePos, inode, sizeof(INode));

        uint16 type = inode->typePermissions & INode_TypeMask;
        switch(type) {
        case INode_TypeFile: node->type = Node::TYPE_FILE; break;
        case INode_TypeDirectory: node->type = Node::TYPE_DIRECTORY; break;
        case INode_TypeCharDev: node->type = Node::TYPE_DEVICE_CHAR; break;
        case INode_TypeBlockDev: node->type = Node::TYPE_DEVICE_BLOCK; break;
        case INode_TypeFIFO: node->type = Node::TYPE_PIPE; break;
        }

        node->dir = nullptr;
        if(node->type == Node::TYPE_FILE)
            node->fileSize = inode->size;
        node->linkRefCount = inode->hardlinkCount;

        node->ownerUID = 0;
        node->ownerGID = 0;
        node->permissions.ownerPermissions = Permissions::Read;
        node->permissions.groupPermissions = Permissions::Read;
        node->permissions.otherPermissions = Permissions::Read;

        node->fsData = inode;
    }
    void Ext2Driver::WriteNode(Node* node) {
    }

    uint64 Ext2Driver::ReadNodeData(Node* node, uint64 pos, void* buffer, uint64 bufferSize) {
        char* realBuffer = (char*)buffer;

        INode* inode = (INode*)node->fsData;

        if(bufferSize > inode->size)
            bufferSize = inode->size;

        uint64 res = bufferSize;

        bool indirectBufferLoaded = false;
        uint8* indirectBuffer = nullptr;

        while(bufferSize > 0) {
            uint64 blockIndex = pos / (1024 << m_SB.blockSizeShift);
            uint64 blockID;

            if(blockIndex < 12) {
                blockID = inode->directPointers[blockIndex];
            } else {
                if(!indirectBufferLoaded) {
                    indirectBuffer = new uint8[1024 << m_SB.blockSizeShift];
                    m_Driver->GetData(m_Dev, inode->indirectPointer * (1024 << m_SB.blockSizeShift), indirectBuffer, 1024 << m_SB.blockSizeShift);
                    indirectBufferLoaded = true;
                }
                blockID = ((uint32*)indirectBuffer)[blockIndex - 12];
            }

            uint64 blockPos = blockID * (1024 << m_SB.blockSizeShift);
            uint64 offset = pos % (1024 << m_SB.blockSizeShift);

            uint64 leftInBlock = (1024 << m_SB.blockSizeShift) - offset;
            if(leftInBlock > bufferSize)
                leftInBlock = bufferSize;

            m_Driver->GetData(m_Dev, blockPos + offset, realBuffer, leftInBlock);

            realBuffer += leftInBlock;
            bufferSize -= leftInBlock;
            pos += leftInBlock;
        }

        if(indirectBufferLoaded)
            delete[] indirectBuffer;

        return res;
    }
    uint64 Ext2Driver::WriteNodeData(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) {
        return 0;
    }

    Directory* Ext2Driver::ReadDirEntries(Node* node) {
        INode* inode = (INode*)node->fsData;

        Directory* dir = Directory::Create(10);

        uint64 pos = 0;
        while(pos < inode->size) {
            uint64 entryBlock = pos / (1024 << m_SB.blockSizeShift);
            uint64 entryBlockPos = inode->directPointers[entryBlock] * (1024 << m_SB.blockSizeShift);
            uint64 entryOffset = pos % (1024 << m_SB.blockSizeShift);
            
            DirEntry ext2Entry;
            m_Driver->GetData(m_Dev, entryBlockPos + entryOffset, &ext2Entry, sizeof(DirEntry));

            DirectoryEntry* vfsEntry;
            Directory::AddEntry(&dir, &vfsEntry);

            vfsEntry->nodeID = ext2Entry.inode;
            m_Driver->GetData(m_Dev, entryBlockPos + entryOffset + sizeof(DirEntry), vfsEntry->name, ext2Entry.nameLengthLow);
            vfsEntry->name[ext2Entry.nameLengthLow] = '\0';

            pos += ext2Entry.entrySize;
        }

        node->dir = dir;
        return dir;
    }
    void Ext2Driver::WriteDirEntries(Node* node) {

    }

    static FileSystem* Ext2Factory(BlockDeviceDriver* driver, uint64 subID) {
        return new Ext2Driver(driver, subID);
    }

    void Ext2Driver::Init() {
        FileSystemRegistry::RegisterFileSystem("ext2", Ext2Factory);
    }

}