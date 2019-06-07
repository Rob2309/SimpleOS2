#pragma once

#include "../FileSystem.h"

namespace VFS {

    struct __attribute__((packed)) ext2superblock {
        uint32 inodeCount;
        uint32 blockCount;
        uint32 reservedBlockCount;
        uint32 freeBlockCount;
        uint32 freeINodeCount;
        uint32 superBlockBlockID;
        uint32 blockSizeShift;
        uint32 fragmentSizeShift;
        uint32 blocksPerGroup;
        uint32 fragmentsPerGroup;
        uint32 inodesPerGroup;
        uint32 lastMountTime;
        uint32 lastWriteTime;
        uint16 mountCountSinceLastCheck;
        uint16 mountCountAllowed;
        uint16 ext2Signature;
        uint16 fsState;
        uint16 errorAction;
        uint16 versionMinor;
        uint32 lastCheckTime;
        uint32 checkInterval;
        uint32 osID;
        uint32 versionMajor;
        uint16 reservedUser;
        uint16 reservedGroup;
        
        uint32 firstUsableINode;
        uint16 inodeSize;
        uint16 blockGroupID;
        uint32 optionalFeatures;
        uint32 requiredFeatures;
        uint32 readonlyFeatures;
        char fsID[16];
        char volumeName[16];
        char lastMountPoint[64];
        uint32 compressionAlg;
        uint8 preallocFiles;
        uint8 preallocDirectory;
        uint16 unused;
        char journalID[16];
        uint32 journalINode;
        uint32 journalDevice;
        uint32 orphanListINode;
    };

    class ext2driver : public FileSystem {
    public:
        ext2driver(const char* dev);

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
        uint64 m_Dev;
        ext2superblock m_SB;
    };

}