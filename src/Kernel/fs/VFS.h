#pragma once

#include "types.h"
#include "Directory.h"
#include "atomic/Atomics.h"
#include "Mutex.h"
#include "FileSystem.h"

namespace VFS {

    class FileSystem;

    struct Node
    {
        Mutex lock;

        enum Type {
            TYPE_FILE,      // Normal File
            TYPE_DIRECTORY, // Directory, containing other nodes
            TYPE_DEVICE,    // Device File
            TYPE_PIPE,      // (Named) Pipe
        } type;

        FileSystem* fs;     // The FileSystem instance this node belongs to
        
        union {
            Directory* dir;
            uint64 fileSize;
            struct {
                uint64 driverID;
                uint64 subID;
            } device;
        };

        uint64 id;
        uint64 linkRefCount;    // How often this node is referenced by directory entries
    };

    struct NodeStats {
        uint64 size;
    };

    void Init(FileSystem* rootFS);
    
    bool CreateFile(const char* path);
    bool CreateFolder(const char* path);

    bool CreateDeviceFile(const char* path, uint64 driverID, uint64 subID);
    bool CreatePipe(uint64* readDesc, uint64* writeDesc);

    /**
     * Removes the files directory entry from the containing directory.
     * The node will, however, only be freed if every reference to it is closed.
     **/
    bool Delete(const char* path);

    /**
     * Mount the given FileSystem at the given path.
     * MountPoint has to be an empty folder.
     **/
    bool Mount(const char* mountPoint, FileSystem* fs);

    /**
     * Unmounts the given path
     **/
    bool Unmount(const char* mountPoint);

    /**
     * Opens the given path
     * Returns the FileDescriptor of the opened path, or 0 on error.
     **/
    uint64 Open(const char* path);
    /**
     * Closes the given FileDescriptor
     **/
    void Close(uint64 desc);

    struct FileList {
        uint64 numEntries;
        
        struct Entry {
            char name[50];
        } *entries;
    };
    /**
     * Lists the files in a directory
     **/
    bool List(const char* path, FileList* list);

    /**
     * Reads from the given File and increases the FileDescriptor position by the number of bytes read.
     * This function blocks until at least one byte was read, except for when it is impossible to read further (e.g. end of file).
     * @param buffer The buffer to read the data into, should be a Kernel pointer
     * @param bufferSize The maximum number of bytes that fit into the given buffer
     * @returns the number of bytes read.
     **/
    uint64 Read(uint64 desc, void* buffer, uint64 bufferSize);
    /**
     * Reads from the given File.
     * This function blocks until at least one byte was read, except for when it is impossible to read further (e.g. end of file).
     * @param pos The position to start reading from
     * @param buffer The buffer to read the data into, should be a Kernel pointer
     * @param bufferSize The maximum number of bytes that fit into the given buffer
     * @returns the number of bytes read.
     **/
    uint64 Read(uint64 desc, uint64 pos, void* buffer, uint64 bufferSize);
    /**
     * Writes to the given File and increases the FileDescriptor position by the number of bytes written.
     * This function blocks until at least one byte was written, except for when it is impossible to write further.
     * @param buffer The buffer to write the data from, should be a Kernel pointer
     * @param bufferSize The number of bytes contained in the given buffer
     * @returns the number of bytes written.
     **/
    uint64 Write(uint64 desc, const void* buffer, uint64 bufferSize);
    /**
     * Writes to the given File.
     * This function blocks until at least one byte was written, except for when it is impossible to write further.
     * @param pos The position within the nodes data to start writing to
     * @param buffer The buffer to write the data from, should be a Kernel pointer
     * @param bufferSize The number of bytes contained in the given buffer
     * @returns the number of bytes written.
     **/
    uint64 Write(uint64 desc, uint64 pos, const void* buffer, uint64 bufferSize);

    /**
     * Retrieves information about the node associated with a FileDescriptor
     * @param desc The FileDescriptor to retrieve information from
     * @param stats The buffer to write the stats into
     **/
    void Stat(uint64 desc, NodeStats* stats);

}