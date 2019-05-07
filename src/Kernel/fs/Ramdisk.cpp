#include "Ramdisk.h"

#include "RamdiskStructs.h"
#include "VFS.h"
#include "terminal/conio.h"
#include "memory/memutil.h"

RamdiskFS::RamdiskFS(const char* dev)
{
    m_DevFile = VFS::OpenNode(dev);
    if(m_DevFile == 0) {
        printf("Ramdisk device file not found (%s)\n", dev);
        return;
    }

    VFS::ReadNode(m_DevFile, 0, &m_Header, sizeof(RamdiskHeader));
}
RamdiskFS::~RamdiskFS()
{
    VFS::CloseNode(m_DevFile);
}

void RamdiskFS::Mount(VFS::Node& mountPoint)
{
    RamdiskFile* files = new RamdiskFile[m_Header.numFiles];
    VFS::ReadNode(m_DevFile, sizeof(RamdiskHeader), files, m_Header.numFiles * sizeof(RamdiskFile));

    mountPoint.directory.numFiles = m_Header.numFiles;
    mountPoint.directory.files = new uint64[m_Header.numFiles];
    for(int i = 0; i < m_Header.numFiles; i++)
    {
        VFS::Node* n = VFS::CreateNode();
        n->type = VFS::Node::TYPE_FILE;
        n->file.size = files[i].size;
        memcpy(n->name, files[i].name, 50);
        n->fs = this;
        n->fsNodeID = files[i].dataOffset;
        n->refCount++;
        mountPoint.directory.files[i] = n->id;
        VFS::ReleaseNode(n);
    }
}
void RamdiskFS::Unmount()
{ }

void RamdiskFS::CreateNode(VFS::Node& node)
{ }
void RamdiskFS::DestroyNode(VFS::Node& node)
{ }

uint64 RamdiskFS::ReadNode(const VFS::Node& node, uint64 pos, void* buffer, uint64 bufferSize)
{
    uint64 rem = node.file.size - pos;
    if(bufferSize < rem)
        rem = bufferSize;

    VFS::ReadNode(m_DevFile, node.fsNodeID + pos, buffer, rem);
    return rem;
}
uint64 RamdiskFS::WriteNode(VFS::Node& node, uint64 pos, void* buffer, uint64 bufferSize)
{ }

void RamdiskFS::ReadDirEntries(VFS::Node& folder)
{ }