#include <cstdio>
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <cstring>

#include "RamdiskStructs.h"

struct File 
{
    std::string name;

    uint64 dataSize;
    uint8* data;
};

int main(int argc, char** argv) 
{
    std::vector<File> files;

    for(int i = 2; i < argc; i += 2) {
        std::string srcFile = argv[i];
        std::string destFile = argv[i+1];

        std::cout << "Adding file " << srcFile << " as " << destFile << std::endl;

        FILE* file = fopen(srcFile.c_str(), "rb");
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        uint8* data = new uint8[size];
        fread(data, 1, size, file);
        fclose(file);

        File f;
        f.name = destFile;
        f.dataSize = size;
        f.data = data;
        files.push_back(f);
    }

    FILE* out = fopen(argv[1], "wb");

    RamdiskHeader header;
    header.numFiles = files.size();
    fwrite(&header, 1, sizeof(RamdiskHeader), out);

    uint64 offset = sizeof(RamdiskHeader) + files.size() * sizeof(RamdiskFile);
    for(File& f : files) {
        RamdiskFile fileHeader;
        memset(fileHeader.name, 0, sizeof(fileHeader.name));
        memcpy(fileHeader.name, f.name.c_str(), f.name.length());
        fileHeader.size = f.dataSize;
        fileHeader.dataOffset = offset;
        offset += f.dataSize;

        fwrite(&fileHeader, 1, sizeof(RamdiskFile), out);
    }

    for(File& f : files) {
        fwrite(f.data, 1, f.dataSize, out);
    }

    fclose(out);
}