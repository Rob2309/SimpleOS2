#include <cstdio>
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <cstring>

#include <types.h>

struct File 
{
    std::string name;

    uint64 dataSize;
    uint8* data;
};

int main(int argc, char** argv) 
{
    std::string outFile = argv[1];

    system(("dd if=/dev/zero of=" + outFile + " bs=512 count=20000 2> /dev/null").c_str());
    system(("mkfs.ext2 " + outFile + " -L SimpleOS2 > /dev/null 2>&1").c_str());

    system(("echo >> " + outFile + ".cmd").c_str());

    for(int i = 2; i < argc; i += 2) {
        std::string srcFile = argv[i];
        std::string destFile = argv[i+1];

        std::cout << "\e[33mAdding file " << srcFile << " as " << destFile << "\e[0m" << std::endl;

        system(("echo write " + srcFile + " " + destFile + " >> " + outFile + ".cmd").c_str());
    }

    system(("echo q >> " + outFile + ".cmd").c_str());

    system(("debugfs -w " + outFile + " -f " + outFile + ".cmd > /dev/null 2>&1").c_str());
}