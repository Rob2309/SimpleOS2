#include "Ramdisk.h"

#include "RamdiskStructs.h"
#include "VFS.h"
#include "conio.h"
#include "memutil.h"

RamdiskFS::RamdiskFS(const char* dev)
{
    m_DevFile = VFS::OpenFile(dev);
    if(m_DevFile == 0)
        printf("Ramdisk device file not found\n");

    VFS::ReadFile(m_DevFile, &m_Header, sizeof(RamdiskHeader));
}
RamdiskFS::~RamdiskFS()
{
    VFS::CloseFile(m_DevFile);
}

void RamdiskFS::Mount(VFS::Node& mountPoint)
{
    RamdiskFile* files = new RamdiskFile[m_Header.numFiles];
    VFS::ReadFile(m_DevFile, files, m_Header.numFiles * sizeof(RamdiskFile));

    mountPoint.directory.numFiles = m_Header.numFiles;
    mountPoint.directory.files = new uint64[m_Header.numFiles];
    for(int i = 0; i < m_Header.numFiles; i++)
    {
        VFS::Node* n = VFS::GetFreeNode();
        n->type = VFS::Node::TYPE_FILE;
        n->file.size = files[i].size;
        memcpy(n->name, files[i].name, 50);
        n->fs = this;
        n->fsNode = files[i].dataOffset;
        mountPoint.directory.files[i] = n->id;
    }
}
void RamdiskFS::Unmount()
{ }

void RamdiskFS::CreateNode(VFS::Node& folder, VFS::Node& node)
{ }
void RamdiskFS::DestroyNode(VFS::Node& folder, VFS::Node& node)
{ }

uint64 RamdiskFS::ReadFile(const VFS::Node& node, uint64 pos, void* buffer, uint64 bufferSize)
{
    uint64 rem = node.file.size - pos;
    if(bufferSize < rem)
        rem = bufferSize;

    VFS::SeekFile(m_DevFile, node.fsNode + pos);
    VFS::ReadFile(m_DevFile, buffer, rem);
    return rem;
}
void RamdiskFS::WriteFile(VFS::Node& node, uint64 pos, void* buffer, uint64 bufferSize)
{ }

void RamdiskFS::ReadDirEntries(VFS::Node& folder)
{ }