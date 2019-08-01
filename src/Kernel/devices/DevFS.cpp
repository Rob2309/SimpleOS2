#include "DevFS.h"

#include "locks/StickyLock.h"
#include "ktl/vector.h"
#include "klib/string.h"
#include "init/Init.h"

#include "klib/stdio.h"

struct DevReg {
    const char* name;
    bool blockDev;
    uint64 driverID;
    uint64 devID;

    uint64 index;
};

static StickyLock g_DevLock;
static ktl::vector<DevReg> g_Devices;
static void* g_InfoPtr = nullptr;

void DevFS::RegisterCharDevice(const char* name, uint64 driverID, uint64 devID) {
    g_DevLock.Spinlock();
    g_Devices.push_back({ name, false, driverID, devID, g_Devices.size() });
    g_DevLock.Unlock();

    if(g_InfoPtr != nullptr) {
        VFS::InvalidateDirEntries(g_InfoPtr, 0xFFFF);
    }
}
void DevFS::RegisterBlockDevice(const char* name, uint64 driverID, uint64 devID) {
    g_DevLock.Spinlock();
    g_Devices.push_back({ name, true, driverID, devID, g_Devices.size() });
    g_DevLock.Unlock();

    if(g_InfoPtr != nullptr) {
        VFS::InvalidateDirEntries(g_InfoPtr, 0xFFFF);
    }
}
void DevFS::UnregisterDevice(uint64 driverID, uint64 devID) {
    g_DevLock.Spinlock();

    for(auto a = g_Devices.begin(); a != g_Devices.end(); ++a) {
        if(a->driverID == driverID && a->devID == devID) {
            g_Devices.erase(a);
            g_DevLock.Unlock();
            return;
        }
    }

    g_DevLock.Unlock();

    if(g_InfoPtr != nullptr) {
        VFS::InvalidateDirEntries(g_InfoPtr, 0xFFFF);
    }
}

DevFS::DevFS() {

}

void DevFS::GetSuperBlock(VFS::SuperBlock* sb, void* infoPtr) {
    sb->rootNode = 0xFFFF;
    g_InfoPtr = infoPtr;
}

void DevFS::CreateNode(VFS::Node* node) { }
void DevFS::DestroyNode(VFS::Node* node) { }

void DevFS::ReadNode(uint64 id, VFS::Node* node) {
    if(id == 0xFFFF) {
        node->type = VFS::Node::TYPE_DIRECTORY;
        node->fs = this;
        node->ownerUID = 0;
        node->ownerGID = 0;
        node->permissions = { 1, 1, 1 };
        node->id = 0xFFFF;
        node->linkRefCount = 1;

        g_DevLock.Spinlock();
        node->dir = VFS::Directory::Create(g_Devices.size());
        for(const DevReg& dr : g_Devices) {
            VFS::DirectoryEntry* entry;
            VFS::Directory::AddEntry(&node->dir, &entry);

            entry->nodeID = dr.index + 1;
            kstrcpy(entry->name, dr.name);
        }
        g_DevLock.Unlock();
    } else {
        g_DevLock.Spinlock();
        const DevReg& dr = g_Devices[id - 1];

        node->type = dr.blockDev ? VFS::Node::TYPE_DEVICE_BLOCK : VFS::Node::TYPE_DEVICE_CHAR;
        node->fs = this;
        node->device.driverID = dr.driverID;
        node->device.subID = dr.devID;
        node->ownerGID = 0;
        node->ownerUID = 0;
        node->permissions = { 1, 1, 1 };
        node->id = id;
        node->linkRefCount = 1;

        g_DevLock.Unlock();
    }
}

void DevFS::WriteNode(VFS::Node* node) { }

uint64 DevFS::ReadNodeData(VFS::Node* node, uint64 pos, void* buffer, uint64 bufferSize) { return 0; }
uint64 DevFS::WriteNodeData(VFS::Node* node, uint64 pos, const void* buffer, uint64 bufferSize) { return 0; }
void DevFS::ClearNodeData(VFS::Node* node) { }

VFS::Directory* DevFS::ReadDirEntries(VFS::Node* node) { 
    node->type = VFS::Node::TYPE_DIRECTORY;
    node->fs = this;
    node->ownerUID = 0;
    node->ownerGID = 0;
    node->permissions = { 1, 1, 1 };
    node->id = 0xFFFF;
    node->linkRefCount = 1;

    g_DevLock.Spinlock();
    node->dir = VFS::Directory::Create(g_Devices.size());
    for(const DevReg& dr : g_Devices) {
        VFS::DirectoryEntry* entry;
        VFS::Directory::AddEntry(&node->dir, &entry);

        entry->nodeID = dr.index + 1;
        kstrcpy(entry->name, dr.name);
    }
    g_DevLock.Unlock();

    return node->dir;
}
void DevFS::WriteDirEntries(VFS::Node* node) {  }


static VFS::FileSystem* DevFSFactory() {
    return new DevFS();
}

static void Init() {
    VFS::FileSystemRegistry::RegisterFileSystem("devFS", DevFSFactory);
}
REGISTER_INIT_FUNC(Init, INIT_STAGE_FSDRIVERS);