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

static StickyLock g_Lock;

static ktl::vector<DevReg> g_Devices;
static VFS::MountPoint* g_MP = nullptr;

static VFS::Directory* g_CurrentDir = nullptr;
static VFS::Directory* g_NewDir = nullptr;

static const char* AppendNumberToName(const char* name, int number) {
    int l = kstrlen(name);
    char* buffer = new char[l + 3 + 1];
    kstrcpy(buffer, name);

    int i = l;
    do {
        buffer[i] = (number % 10) + '0';
        number /= 10;
        i++;
    } while(number > 0);

    buffer[i] = '\0';

    return buffer;
}

static bool CheckUnique(const char* name) {
    for(const auto& dev : g_Devices) {
        if(kstrcmp(dev.name, name) == 0)
            return false;
    }

    return true;
}

static const char* MakeUniqueName(const char* name) {
    int number = 0;

    const char* uniqueName = nullptr;

    while(true) {
        const char* tempName = AppendNumberToName(name, number);

        if(CheckUnique(tempName)) {
            uniqueName = tempName;
            break;
        }

        delete[] tempName;
        number++;
    }

    return uniqueName;
}

static void _RegisterDevice(const char* uniqueName, bool blockDev, uint64 driverID, uint64 devID) {
    if(g_NewDir == nullptr)
        g_NewDir = VFS::Directory::Create(10);

    VFS::DirectoryEntry* entry;
    VFS::Directory::AddEntry(&g_NewDir, &entry);

    kstrcpy(entry->name, uniqueName);
    entry->nodeID = g_Devices.size() + 1;

    g_Devices.push_back({ uniqueName, blockDev, driverID, devID, g_Devices.size() });
}

void DevFS::RegisterCharDevice(const char* name, uint64 driverID, uint64 devID) {
    g_Lock.Spinlock();

    const char* uniqueName = MakeUniqueName(name);

    _RegisterDevice(uniqueName, false, driverID, devID);

    g_Lock.Unlock();
}
void DevFS::RegisterBlockDevice(const char* name, uint64 driverID, uint64 devID) {
    g_Lock.Spinlock();

    const char* uniqueName = MakeUniqueName(name);

    _RegisterDevice(uniqueName, true, driverID, devID);

    g_Lock.Unlock();
}

void DevFS::RegisterUniqueCharDevice(const char* uniqueName, uint64 driverID, uint64 devID) {
    g_Lock.Spinlock();

    _RegisterDevice(uniqueName, false, driverID, devID);

    g_Lock.Unlock();
}
void DevFS::RegisterUniqueBlockDevice(const char* uniqueName, uint64 driverID, uint64 devID) {
    g_Lock.Spinlock();

    _RegisterDevice(uniqueName, true, driverID, devID);

    g_Lock.Unlock();
}

void DevFS::UnregisterDevice(uint64 driverID, uint64 devID) {
    
}

DevFS::DevFS() {
    g_CurrentDir = VFS::Directory::Create(10);
}

void DevFS::GetSuperBlock(VFS::SuperBlock* sb) {
    sb->rootNode = 0xFFFF;
}
void DevFS::SetMountPoint(VFS::MountPoint* mp) { g_MP = mp; }
void DevFS::PrepareUnmount() { }

void DevFS::CreateNode(VFS::Node* node) { }
void DevFS::DestroyNode(VFS::Node* node) { }

void DevFS::UpdateDir(VFS::Node* node) {
    g_Lock.Spinlock();

    if(g_NewDir != nullptr) {
        VFS::Directory::Destroy(g_CurrentDir);
        g_CurrentDir = g_NewDir;
        g_NewDir = nullptr;
    }

    node->infoFolder.cachedDir = g_CurrentDir;

    g_Lock.Unlock();
}

void DevFS::ReadNode(uint64 id, VFS::Node* node) {
    if(id == 0xFFFF) {
        node->type = VFS::Node::TYPE_DIRECTORY;
        node->ownerUID = 0;
        node->ownerGID = 0;
        node->permissions = { 1, 1, 1 };
        node->id = 0xFFFF;
        node->linkCount = 1;

        g_Lock.Spinlock();
        
        if(g_NewDir != nullptr) {
            VFS::Directory::Destroy(g_CurrentDir);
            g_CurrentDir = g_NewDir;
            g_NewDir = nullptr;
        }

        node->infoFolder.cachedDir = g_CurrentDir;

        g_Lock.Unlock();
    } else {
        g_Lock.Spinlock();
        const DevReg& dr = g_Devices[id - 1];

        node->type = dr.blockDev ? VFS::Node::TYPE_DEVICE_BLOCK : VFS::Node::TYPE_DEVICE_CHAR;
        node->infoDevice.driverID = dr.driverID;
        node->infoDevice.subID = dr.devID;
        node->ownerGID = 0;
        node->ownerUID = 0;
        node->permissions = { 1, 1, 1 };
        node->id = id;
        node->linkCount = 1;

        g_Lock.Unlock();
    }
}

void DevFS::WriteNode(VFS::Node* node) { }

uint64 DevFS::ReadNodeData(VFS::Node* node, uint64 pos, void* buffer, uint64 bufferSize) { return 0; }
uint64 DevFS::WriteNodeData(VFS::Node* node, uint64 pos, const void* buffer, uint64 bufferSize) { return 0; }
void DevFS::ClearNodeData(VFS::Node* node) { }


static VFS::FileSystem* DevFSFactory() {
    return new DevFS();
}

static void Init() {
    VFS::FileSystemRegistry::RegisterFileSystem("devFS", DevFSFactory);
}
REGISTER_INIT_FUNC(Init, INIT_STAGE_FSDRIVERS);