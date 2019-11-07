#pragma once

#include "../FileSystem.h"

namespace Ext2 {

    struct __attribute__((packed)) SuperBlock {
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

    constexpr uint16 RequiredFeature_Compression = 0x0001;
    constexpr uint16 RequiredFeature_DirectoryType = 0x0002;
    constexpr uint16 RequiredFeature_ReplayJournal = 0x0004;
    constexpr uint16 RequiredFeature_Journal = 0x0008;

    struct __attribute__((packed)) BlockGroupDesc {
        uint32 blockBitmapBlock;
        uint32 inodeBitmapBlock;
        uint32 inodeTableBlock;
        uint16 freeBlocks;
        uint16 freeINodes;
        uint16 numDirectories;
    };

    struct __attribute__((packed)) INode {
        uint16 typePermissions;
        uint16 userID;
        uint32 size;
        uint32 accessTime;
        uint32 creationTime;
        uint32 modificationTime;
        uint32 deletionTime;
        uint16 groupID;
        uint16 hardlinkCount;
        uint32 sectorCount;
        uint32 flags;
        uint32 osVal1;
        uint32 directPointers[12];
        uint32 indirectPointer;
        uint32 doubleIndirectPointer;
        uint32 tripleIndirectPointer;
        uint32 generationNumber;
        uint32 fileACL;
        union {
            uint32 sizeHigh;
            uint32 directoryACL;
        };
        uint32 fragmentBlock;
        char osVal2[12];
    };

    constexpr uint16 INode_TypeFIFO = 0x1000;
    constexpr uint16 INode_TypeCharDev = 0x2000;
    constexpr uint16 INode_TypeDirectory = 0x4000;
    constexpr uint16 INode_TypeBlockDev = 0x6000;
    constexpr uint16 INode_TypeFile = 0x8000;
    constexpr uint16 INode_TypeSymLink = 0xA000;
    constexpr uint16 INode_TypeUnixSocket = 0xC000;
    constexpr uint16 INode_TypeMask = 0xF000;

    struct __attribute__((packed)) DirEntry {
        uint32 inode;
        uint16 entrySize;
        uint8 nameLengthLow;
        union {
            uint8 nameLengthHigh;
            uint8 type;
        };
        char name[];
    };

    constexpr uint8 DirEntry_TypeUnknown = 0;
    constexpr uint8 DirEntry_TypeFile = 1;
    constexpr uint8 DirEntry_TypeDirectory = 2;
    constexpr uint8 DirEntry_TypeCharDev = 3;
    constexpr uint8 DirEntry_TypeBlockDev = 4;
    constexpr uint8 DirEntry_TypeFIFO = 5;
    constexpr uint8 DirEntry_TypeSocket = 6;
    constexpr uint8 DirEntry_TypeSymlink = 7;

    class Ext2Driver : public VFS::FileSystem {
    public:
        Ext2Driver(BlockDeviceDriver* driver, uint64 subID);

        void GetSuperBlock(VFS::SuperBlock* sb) override;
        void SetMountPoint(VFS::MountPoint* mp) override;
        void PrepareUnmount() override;

        void CreateNode(VFS::Node* node) override;
        void DestroyNode(VFS::Node* node) override;

        void UpdateDir(VFS::Node* node) override;

        void ReadNode(uint64 id, VFS::Node* node) override; 
        void WriteNode(VFS::Node* node) override;

        uint64 ReadNodeData(VFS::Node* node, uint64 pos, void* buffer, uint64 bufferSize) override;
        uint64 WriteNodeData(VFS::Node* node, uint64 pos, const void* buffer, uint64 bufferSize) override;
        void ClearNodeData(VFS::Node* node) override;

    private:
        VFS::MountPoint* m_MP;
        BlockDeviceDriver* m_Driver;
        uint64 m_Dev;
        SuperBlock m_SB;
        uint64 m_BlockSize;
        uint64 m_BlockGroupDescTableOffset;
    };

}