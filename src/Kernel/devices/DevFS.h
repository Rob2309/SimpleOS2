#pragma once

#include "fs/FileSystem.h"

class DevFS : public VFS::FileSystem {
public:
    static void RegisterCharDevice(const char* name, uint64 driverID, uint64 devID);
    static void RegisterBlockDevice(const char* name, uint64 driverID, uint64 devID);
    static void UnregisterDevice(uint64 driverID, uint64 devID);

public:
    DevFS();

    void GetSuperBlock(VFS::SuperBlock* sb) override;
    void SetMountPoint(VFS::MountPoint* mp) override;
    void PrepareUnmount() override;

    void UpdateDir(VFS::Node* node) override;

    void CreateNode(VFS::Node* node) override;
    void DestroyNode(VFS::Node* node) override;

    void ReadNode(uint64 id, VFS::Node* node) override;
    void WriteNode(VFS::Node* node) override;

    uint64 ReadNodeData(VFS::Node* node, uint64 pos, void* buffer, uint64 bufferSize) override;
    uint64 WriteNodeData(VFS::Node* node, uint64 pos, const void* buffer, uint64 bufferSize) override;
    void ClearNodeData(VFS::Node* node) override;
};