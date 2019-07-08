#include "TestFS.h"

#include "klib/memory.h"

using namespace VFS;

struct TestNode {
    Node::Type type;
    Directory* dir;
    uint64 linkRefCount;
    
    uint64 fileSize;
    char* fileData;
};

TestFS::TestFS() {
    TestNode* node = new TestNode();
    node->type = Node::TYPE_DIRECTORY;
    node->dir = Directory::Create(10);
    node->linkRefCount = 1;
    
    m_RootNodeID = (uint64)node;
}

void TestFS::GetSuperBlock(SuperBlock* sb) {
    sb->rootNode = m_RootNodeID;
}

void TestFS::CreateNode(Node* node) {
    TestNode* newNode = new TestNode();
    newNode->dir = nullptr;
    newNode->linkRefCount = 0;
    newNode->fileSize = 0;
    newNode->fileData = nullptr;
    
    node->id = (uint64)newNode;
    node->dir = newNode->dir;
    node->fs = this;
    node->linkRefCount = newNode->linkRefCount;
}
void TestFS::DestroyNode(Node* node) {
    TestNode* oldNode = (TestNode*)(node->id);
    if(oldNode->type == Node::TYPE_FILE && oldNode->fileData != nullptr)
        delete[] oldNode->fileData;
    delete oldNode;
}

void TestFS::ReadNode(uint64 id, VFS::Node* node) {
    TestNode* refNode = (TestNode*)id;

    node->dir = refNode->dir;
    node->fs = this;
    node->id = id;
    node->linkRefCount = refNode->linkRefCount;
    node->type = refNode->type;
}
void TestFS::WriteNode(Node* node) {
    TestNode* refNode = (TestNode*)(node->id);

    refNode->dir = node->dir;
    refNode->type = node->type;
    refNode->linkRefCount = node->linkRefCount;
}

uint64 TestFS::ReadNodeData(Node* node, uint64 pos, void* buffer, uint64 bufferSize) {
    TestNode* refNode = (TestNode*)(node->id);

    uint64 rem = refNode->fileSize - pos;
    if(rem > bufferSize)
        rem = bufferSize;

    kmemcpy_usersafe(buffer, refNode->fileData + pos, rem);
    return rem;
}
uint64 TestFS::WriteNodeData(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) {
    TestNode* refNode = (TestNode*)(node->id);

    uint64 cap = refNode->fileSize - pos;
    if(cap < bufferSize) {
        char* newData = new char[pos + bufferSize];
        kmemcpy(newData, refNode->fileData, refNode->fileSize);
        delete[] refNode->fileData;
        refNode->fileData = newData;
        refNode->fileSize = pos + bufferSize;
    }

    kmemcpy_usersafe(refNode->fileData + pos, buffer, bufferSize);
    return bufferSize;
}

Directory* TestFS::ReadDirEntries(Node* node) {
    TestNode* refNode = (TestNode*)(node->id);
    return refNode->dir;
}
void TestFS::WriteDirEntries(Node* node) {
    TestNode* refNode = (TestNode*)(node->id);
    refNode->dir = node->dir;
}