#pragma once

#include "types.h"
#include "Mutex.h"

namespace VFS {

    constexpr uint64 PipeBufferSize = 4096;

    class FileSystem;

    struct Node
    {
        enum Type {
            TYPE_FILE,      // Normal File
            TYPE_DIRECTORY, // Directory, containing other nodes
            TYPE_DEVICE,    // Device File

            TYPE_PIPE,      // Pipe File
        } type;

        union {
            struct {
                uint64 size;
            } file;
            struct {
                uint64 numFiles;
                uint64* files;
            } directory;
            struct {
                uint64 devID;
            } device;
            struct {
                uint64 head;
                uint64 tail;
                char* buffer;
            } pipe;
        };

        char name[50];

        FileSystem* fs;     // The FileSystem instance this node belongs to
        uint64 fsNodeID;    // The nodeID the FileSystem associates with this node

        uint64 id;
        uint64 refCount;    // How often this node is referenced globally

        Mutex lock;
    };

    class FileSystem
    {
    public:
        virtual void Mount(Node& mountPoint) = 0;
        virtual void Unmount() = 0;

        virtual void CreateNode(Node& node) = 0;
        virtual void DestroyNode(Node& node) = 0;

        virtual uint64 ReadNode(const Node& node, uint64 pos, void* buffer, uint64 bufferSize) = 0;
        virtual uint64 WriteNode(Node& node, uint64 pos, void* buffer, uint64 bufferSize) = 0;
    
        virtual void ReadDirEntries(Node& folder) = 0;
    };

    void Init();
    
    bool CreateFile(const char* folder, const char* name);
    bool CreateFolder(const char* folder, const char* name);
    bool CreateDeviceFile(const char* folder, const char* name, uint64 devID);
    /**
     * Creates a pipe node. The returned node will have a refCount of two, so does not need opening
     **/
    uint64 CreatePipe();
    /**
     * Removes the files directory entry from the containing directory.
     * The node will, however, only be freed if every reference to it is closed.
     **/
    bool DeleteFile(const char* file);

    /**
     * Mount the given FileSystem at the given path.
     * MountPoint has to be a folder.
     **/
    void Mount(const char* mountPoint, FileSystem* fs);

    /**
     * Increments the refCount of the node associated with the given path.
     * Returns the opened NodeID.
     **/
    uint64 OpenNode(const char* path);
    /**
     * Decrements the refCount of the given node.
     **/
    uint64 CloseNode(uint64 node);

    /**
     * Gets the file size of the given node. Only supported for Regular file nodes
     **/
    uint64 GetSize(uint64 node);
    /**
     * Reads from the given node.
     * This function blocks until at least one byte was read, except for when it is impossible to read further (e.g. end of file).
     * @param pos The position within the nodes data to start reading from
     * @param buffer The buffer to read the data into, should be a Kernel pointer
     * @param bufferSize The maximum number of bytes that fit into the given buffer
     * @returns the number of bytes read.
     **/
    uint64 ReadNode(uint64 node, uint64 pos, void* buffer, uint64 bufferSize);
    /**
     * Writes to the given node.
     * This function blocks until at least one byte was written, except for when it is impossible to write further.
     * @param pos The position within the nodes data to start writing to
     * @param buffer The buffer to write the data from, should be a Kernel pointer
     * @param bufferSize The number of bytes contained in the given buffer
     * @returns the number of bytes written.
     **/
    uint64 WriteNode(uint64 node, uint64 pos, void* buffer, uint64 bufferSize);

    /**
     * Locks the node with the given ID and increases its refCount.
     **/
    Node* AcquireNode(uint64 id);
    /**
     * Returns a new Node, nodes with a refCount of zero can be reused.
     * The node will be acquired when returned.
     **/
    Node* CreateNode();
    /**
     * Opposite of AcquireNode
     **/
    void ReleaseNode(Node* node);

}