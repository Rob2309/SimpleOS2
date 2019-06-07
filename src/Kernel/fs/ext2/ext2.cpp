#include "ext2.h"

#include "klib/stdio.h"
#include "klib/string.h"

namespace VFS {

    ext2driver::ext2driver(const char* dev) {
        m_Dev = VFS::Open(dev);
        if(m_Dev == 0)
            kprintf("Failed to open %s\n", dev);

        VFS::Read(m_Dev, 1024, &m_SB, sizeof(ext2superblock));

        kprintf("Ramdisk has version %i.%i\n", m_SB.versionMajor, m_SB.versionMinor);
        kprintf("Volume name: %s\n", m_SB.volumeName);
    }

    void ext2driver::GetSuperBlock(SuperBlock* sb) {
        sb->rootNode = 2;
    }

    void ext2driver::CreateNode(Node* node) {
    }
    void ext2driver::DestroyNode(Node* node) {
    }

    void ext2driver::ReadNode(uint64 id, Node* node) {
        
    }
    void ext2driver::WriteNode(Node* node) {
    }

    uint64 ext2driver::ReadNodeData(Node* node, uint64 pos, void* buffer, uint64 bufferSize) {
        
    }
    uint64 ext2driver::WriteNodeData(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) {
        return 0;
    }

    Directory* ext2driver::ReadDirEntries(Node* node) {

    }
    Directory* ext2driver::WriteDirEntries(Node* node) {

    }

}