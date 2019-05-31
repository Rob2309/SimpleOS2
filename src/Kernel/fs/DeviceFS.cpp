#include "DeviceFS.h"

#include <Mutex.h>
#include "stl/ArrayList.h"
#include "devices/Device.h"

namespace VFS {

    struct DeviceNode {
        uint64 devID;
    };

    static Mutex g_Lock;
    static ArrayList<DeviceNode> g_Nodes;

    void DeviceFS::GetSuperBlock(SuperBlock* sb) {
        sb->rootNode = (uint64)-1;
    }

    void DeviceFS::CreateNode(Node* node) {
        g_Lock.SpinLock();
        node->id = g_Nodes.size();
        node->fs = this;
        g_Nodes.push_back({ (uint64)node->dir });
        g_Lock.Unlock();
    }
    void DeviceFS::DestroyNode(Node* node) {
    }

    void DeviceFS::ReadNode(uint64 id, Node* node) {
        node->fs = this;
        node->id = id;
        node->linkRefCount = 1;
        if(id == (uint64)-1) {
            node->type = Node::TYPE_DIRECTORY;
            node->dir = Directory::Create(10);
        } else {
            node->type = Node::TYPE_DEVICE;
        }
    }
    void DeviceFS::WriteNode(Node* node) {
    }

    uint64 DeviceFS::ReadNodeData(Node* node, uint64 pos, void* buffer, uint64 bufferSize) {
        g_Lock.SpinLock();
        uint64 devID = g_Nodes[node->id].devID;
        g_Lock.Unlock();

        Device* dev = Device::GetByID(devID);
        return dev->Read(pos, buffer, bufferSize);
    }
    uint64 DeviceFS::WriteNodeData(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) {
        g_Lock.SpinLock();
        uint64 devID = g_Nodes[node->id].devID;
        g_Lock.Unlock();

        Device* dev = Device::GetByID(devID);
        return dev->Write(pos, (void*)buffer, bufferSize);
    }

    Directory* DeviceFS::ReadDirEntries(Node* node) {
    }
    Directory* DeviceFS::WriteDirEntries(Node* node) {
    }

}