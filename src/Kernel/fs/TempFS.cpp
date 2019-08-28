#include "TempFS.h"

#include "klib/memory.h"

#include "init/Init.h"

struct TestNode {
    VFS::Node::Type type;
    VFS::Directory* dir;
    uint64 linkRefCount;

    uint64 uid;
    uint64 gid;
    VFS::Permissions perms;
    
    uint64 fileSize;
    char* fileData;
};

static VFS::FileSystem* TempFSFactory() {
    return new TempFS();
}

static void Init() {
    VFS::FileSystemRegistry::RegisterFileSystem("tempfs", TempFSFactory);
}
REGISTER_INIT_FUNC(Init, INIT_STAGE_FSDRIVERS);

using namespace VFS;

TempFS::TempFS() {
    TestNode* node = new TestNode();
    node->type = Node::TYPE_DIRECTORY;
    node->dir = Directory::Create(10);
    node->linkRefCount = 1;
    node->uid = 0;
    node->gid = 0;
    node->perms.ownerPermissions = Permissions::Read | Permissions::Write;
    node->perms.groupPermissions = Permissions::Read;
    node->perms.otherPermissions = Permissions::Read;
    
    m_RootNodeID = (uint64)node;
}

void TempFS::GetSuperBlock(SuperBlock* sb) {
    sb->rootNode = m_RootNodeID;
}
void TempFS::SetMountPoint(MountPoint* mp) {
    m_MP = mp;
}
void TempFS::PrepareUnmount() {

}

void TempFS::CreateNode(Node* node) {
    TestNode* newNode = new TestNode();
    newNode->dir = nullptr;
    newNode->linkRefCount = 0;
    newNode->fileSize = 0;
    newNode->fileData = nullptr;
    
    node->id = (uint64)newNode;
    node->linkCount.Write(0);
    node->ready = true;
}
void TempFS::DestroyNode(Node* node) {
    TestNode* oldNode = (TestNode*)(node->id);
    if(oldNode->type == Node::TYPE_FILE && oldNode->fileData != nullptr)
        delete[] oldNode->fileData;
    delete oldNode;
}

void TempFS::ReadNode(uint64 id, VFS::Node* node) {
    TestNode* refNode = (TestNode*)id;

    node->infoFolder.cachedDir = refNode->dir;
    node->mp = m_MP;
    node->id = id;
    node->linkCount = refNode->linkRefCount;
    node->type = refNode->type;
    node->ownerGID = refNode->gid;
    node->ownerUID = refNode->uid;
    node->permissions = refNode->perms;
    node->ready = true;
}
void TempFS::WriteNode(Node* node) {
    TestNode* refNode = (TestNode*)(node->id);

    refNode->dir = node->infoFolder.cachedDir;
    refNode->type = node->type;
    refNode->linkRefCount = node->linkCount.Read();
    refNode->gid = node->ownerGID;
    refNode->uid = node->ownerUID;
    refNode->perms = node->permissions;
}

void TempFS::UpdateDir(VFS::Node* node) {

}

uint64 TempFS::ReadNodeData(Node* node, uint64 pos, void* buffer, uint64 bufferSize) {
    TestNode* refNode = (TestNode*)(node->id);

    uint64 rem = refNode->fileSize - pos;
    if(rem == 0) // eof
        return 0;
    if(rem > bufferSize)
        rem = bufferSize;

    if(!kmemcpy_usersafe(buffer, refNode->fileData + pos, rem))
        return ErrorInvalidBuffer;
    return rem;
}
uint64 TempFS::WriteNodeData(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) {
    TestNode* refNode = (TestNode*)(node->id);

    uint64 cap = refNode->fileSize - pos;
    if(cap < bufferSize) {
        char* newData = new char[pos + bufferSize];
        kmemcpy(newData, refNode->fileData, refNode->fileSize);
        delete[] refNode->fileData;
        refNode->fileData = newData;
        refNode->fileSize = pos + bufferSize;
    }

    if(!kmemcpy_usersafe(refNode->fileData + pos, buffer, bufferSize))
        return ErrorInvalidBuffer;
    return bufferSize;
}

void TempFS::ClearNodeData(VFS::Node* node) {
    TestNode* refNode = (TestNode*)(node->id);

    delete[] refNode->fileData;
    refNode->fileSize = 0;
    refNode->fileData = nullptr;
}