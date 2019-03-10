#include "Ramdisk.h"

#include "RamdiskStructs.h"
#include "string.h"
#include "VFS.h"
#include "conio.h"

namespace Ramdisk {

    static uint64 g_DevFile;
    static RamdiskHeader g_Header;
    static RamdiskFile* g_Files;

    void Init(const char* dev)
    {
        g_DevFile = VFS::OpenFile(dev);
        if(g_DevFile == 0)
            printf("Failed to init Ramdisk driver\n");

        VFS::SeekFile(g_DevFile, 0);
        VFS::ReadFile(g_DevFile, &g_Header, sizeof(g_Header));

        g_Files = new RamdiskFile[g_Header.numFiles];
        VFS::ReadFile(g_DevFile, g_Files, g_Header.numFiles * sizeof(RamdiskFile));
    }

    File GetFileData(const char* name)
    {
        for(int i = 0; i < g_Header.numFiles; i++) {
            if(strcmp(g_Files[i].name, name) == 0) {
                File res;
                res.size = g_Files[i].size;
                res.data = new uint8[res.size];

                VFS::SeekFile(g_DevFile, g_Files[i].dataOffset);
                VFS::ReadFile(g_DevFile, res.data, res.size);

                return res;
            }
        }

        return { 0, nullptr };
    }

}