#pragma once

#include "types.h"
#include "Directory.h"
#include "atomic/Atomics.h"
#include "locks/QueueLock.h"
#include "FileSystem.h"
#include "user/User.h"

namespace VFS {

    class FileSystem;

    struct Permissions {
        static constexpr uint8 Read = 0x1;
        static constexpr uint8 Write = 0x2;

        uint8 ownerPermissions;
        uint8 groupPermissions;
        uint8 otherPermissions;
    };

    struct Node
    {
        QueueLock lock;

        enum Type {
            TYPE_FILE,              // Normal File
            TYPE_DIRECTORY,         // Directory, containing other nodes
            TYPE_DEVICE_CHAR,       // Character Device File
            TYPE_DEVICE_BLOCK,      // Block Device File
            TYPE_PIPE,              // (Named) Pipe
        } type;

        FileSystem* fs;     // The FileSystem instance this node belongs to
        void* fsData;

        union {
            Directory* dir;
            uint64 fileSize;
            struct {
                uint64 driverID;
                uint64 subID;
            } device;
        };

        uint64 ownerUID;
        uint64 ownerGID;
        Permissions permissions;

        uint64 id;
        uint64 linkRefCount;    // How often this node is referenced by directory entries
    };

    struct NodeStats {
        uint64 ownerUID;
        uint64 ownerGID;
        Permissions permissions;

        uint64 size;
    };

    /**
     * Initializes the VFS and mounts rootFS to /
     **/
    void Init(FileSystem* rootFS);
    
    constexpr int64 OK = 0;
    constexpr int64 ErrorFileNotFound = -1;
    constexpr int64 ErrorPermissionDenied = -2;
    constexpr int64 ErrorInvalidFD = -3;
    constexpr int64 ErrorInvalidBuffer = -4;
    constexpr int64 ErrorInvalidPath = -5;
    constexpr int64 ErrorInvalidFileSystem = -6;

    const char* ErrorToString(int64 error);

    /**
     * Creates a regular file at the given path. 
     * All directories up to the given path have to exist.
     **/
    int64 CreateFile(User* user, const char* path, const Permissions& permissions);
    /**
     * Creates an empty directory.
     * All directories up to the given path have to exist.
     **/
    int64 CreateFolder(User* user, const char* path, const Permissions& permissions);
    /**
     * Creates a special device file.
     * All directories up to the given path have to exist.
     * @param driverID The ID of the driver the device is handled by.
     * @param subID The ID of the device within the driver.
     **/
    int64 CreateDeviceFile(User* user, const char* path, const Permissions& permissions, uint64 driverID, uint64 subID);
    /**
     * Creates an unnamed pipe.
     * @param readDesc Filled with a FileDescriptor that can be used to read from the Pipe.
     * @param writeDesc Filled with a FileDescriptor that can be used to write to the Pipe.
     **/
    int64 CreatePipe(User* user, uint64* readDesc, uint64* writeDesc);

    /**
     * Removes the files directory entry from the containing directory.
     * The node will, however, only be freed if every reference to it is closed.
     **/
    int64 Delete(User* user, const char* path);

    int64 ChangeOwner(User* user, const char* path, uint64 newUID, uint64 newGID);
    int64 ChangePermissions(User* user, const char* path, const Permissions& permissions);

    /**
     * Mount the given FileSystem at the given path.
     * MountPoint has to be an empty folder.
     **/
    int64 Mount(User* user, const char* mountPoint, FileSystem* fs);
    int64 Mount(User* user, const char* mountPoint, const char* fsID);
    int64 Mount(User* user, const char* mountPoint, const char* fsID, const char* dev);

    /**
     * Unmounts the given path
     **/
    int64 Unmount(User* user, const char* mountPoint);

    /**
     * Opens the given path
     * Returns the FileDescriptor of the opened path, or 0 on error.
     **/
    int64 Open(User* user, const char* path, uint8 reqPermissions, uint64& fileDesc);
    /**
     * Closes the given FileDescriptor
     **/
    int64 Close(uint64 desc);
    /**
     * Increments the refCount of the given filedescriptor
     */
    int64 AddRef(uint64 desc);

    struct FileList {
        uint64 numEntries;
        
        struct Entry {
            char name[50];
            Node::Type type;
        } *entries;
    };
    /**
     * Lists the files in a directory, fails if the given path is not a directory.
     **/
    int64 List(User* user, const char* path, FileList* list, bool getTypes);

    /**
     * Reads from the given File and increases the FileDescriptor position by the number of bytes read.
     * This function blocks until at least one byte was read, except for when it is impossible to read further (e.g. end of file).
     * @param buffer The buffer to read the data into, should be a Kernel pointer
     * @param bufferSize The maximum number of bytes that fit into the given buffer
     * @returns the number of bytes read.
     **/
    int64 Read(uint64 desc, void* buffer, uint64 bufferSize);
    /**
     * Reads from the given File.
     * This function blocks until at least one byte was read, except for when it is impossible to read further (e.g. end of file).
     * @param pos The position to start reading from
     * @param buffer The buffer to read the data into, should be a Kernel pointer
     * @param bufferSize The maximum number of bytes that fit into the given buffer
     * @returns the number of bytes read.
     **/
    int64 Read(uint64 desc, uint64 pos, void* buffer, uint64 bufferSize);
    /**
     * Writes to the given File and increases the FileDescriptor position by the number of bytes written.
     * This function blocks until at least one byte was written, except for when it is impossible to write further.
     * @param buffer The buffer to write the data from, should be a Kernel pointer
     * @param bufferSize The number of bytes contained in the given buffer
     * @returns the number of bytes written.
     **/
    int64 Write(uint64 desc, const void* buffer, uint64 bufferSize);
    /**
     * Writes to the given File.
     * This function blocks until at least one byte was written, except for when it is impossible to write further.
     * @param pos The position within the nodes data to start writing to
     * @param buffer The buffer to write the data from, should be a Kernel pointer
     * @param bufferSize The number of bytes contained in the given buffer
     * @returns the number of bytes written.
     **/
    int64 Write(uint64 desc, uint64 pos, const void* buffer, uint64 bufferSize);

    /**
     * Retrieves information about the node associated with a FileDescriptor
     * @param desc The FileDescriptor to retrieve information from
     * @param stats The buffer to write the stats into
     **/
    int64 Stat(uint64 desc, NodeStats* stats);

}