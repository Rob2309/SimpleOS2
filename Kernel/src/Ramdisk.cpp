#include "Ramdisk.h"

#include "RamdiskStructs.h"
#include "string.h"

namespace Ramdisk {

    static uint8* g_RD;

    void Init(uint8* img)
    {
        g_RD = img;
    }

    File GetFileData(const char* name)
    {
        RamdiskHeader* header = (RamdiskHeader*)g_RD;

        for(int i = 0; i < header->numFiles; i++) {
            if(strcmp(header->files[i].name, name) == 0) {
                File res;
                res.size = header->files[i].size;
                res.data = g_RD + header->files[i].dataOffset;
                return res;
            }
        }

        return { 0, nullptr };
    }

}