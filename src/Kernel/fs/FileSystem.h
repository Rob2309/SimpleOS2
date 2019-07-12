#pragma once

#include "types.h"
#include "VFS.h"
#include "Directory.h"
#include "devices/DeviceDriver.h"

namespace VFS {

    struct Node;

    struct SuperBlock {
        uint64 rootNode;
    };

    class FileSystem
    {
    public:
        /**
         * Reads the FileSystems SuperBlock
         **/
        virtual void GetSuperBlock(SuperBlock* sb) = 0;
        
        /**
         * Seeks for a suitable free Node and creates a new Node out of it
         **/
        virtual void CreateNode(Node* node) = 0;
        /**
         * Destroys the given node and marks it free
         **/
        virtual void DestroyNode(Node* node) = 0;

        /**
         * Reads the Node with the given ID and writes its data into node.
         * Will only get called if a referenced node is not in the VFS node cache.
         **/
        virtual void ReadNode(uint64 id, Node* node) = 0; 
        /**
         * Writes the given node.
         * Will only get called if a node is ejected from the VFS node cache.
         **/
        virtual void WriteNode(Node* node) = 0;

        /**
         * Reads data from the given File node.
         * node will not be locked when given to this function.
         **/
        virtual uint64 ReadNodeData(Node* node, uint64 pos, void* buffer, uint64 bufferSize) = 0;
        /**
         * Writes data to the given File node.
         * node will not be locked when given to this function.
         **/
        virtual uint64 WriteNodeData(Node* node, uint64 pos, const void* buffer, uint64 bufferSize) = 0;

        /**
         * Reads the Directory entries from the given node.
         * Will only be called if the directory data is not in the VFS cache.
         * Node will be locked when given to this function.
         **/
        virtual Directory* ReadDirEntries(Node* node) = 0;
        /**
         * Writes the Directory entries to the given node.
         * Will only be called if the directory data gets ejected from the VFS cache.
         * Node will be locked when given to this function.
         **/
        virtual void WriteDirEntries(Node* node) = 0;
    };

    typedef FileSystem* (*FileSystemFactory)();
    typedef FileSystem* (*FileSystemFactoryDev)(BlockDeviceDriver* dev, uint64 subID);

    class FileSystemRegistry {
    public:
        static void RegisterFileSystem(const char* id, FileSystemFactory factory);
        static void RegisterFileSystem(const char* id, FileSystemFactoryDev factory);
        static void UnregisterFileSystem(const char* id);

        static FileSystem* CreateFileSystem(const char* id);
        static FileSystem* CreateFileSystem(const char* id, BlockDeviceDriver* dev, uint64 subID);
    };

}