#include "RamdiskFS.h"

Node* RamdiskFS::CreateFolderNode() {
    Node* node = new Node();
    node->type = Node::TYPE_DIRECTORY;
    node->fs = this;
    node->dir = Directory::Create(16);
    node->id = (uint64)node;
    node->refCount = 0;
    return node;
}

void RamdiskFS::GetSuperBlock(SuperBlock* sb) {
    Node* rootNode = CreateFolderNode();
    rootNode->refCount = 1;
    sb->rootNode = rootNode->id;
}

void RamdiskFS::CreateNode(Node* node) {
    Node* n = new Node();
    n->type = Node::TYPE_FILE;
    n->fs = this;
    n->dir = nullptr;
    n->id = (uint64)n;
    n->refCount = 0;
}