#pragma once

#include "types.h"
#include "Directory.h"
#include "atomic/Atomics.h"
#include "locks/QueueLock.h"
#include "FileSystem.h"
#include "user/User.h"
#include "errno.h"
#include "Node.h"
#include "Permissions.h"

namespace VFS {

    class FileSystem;

    /**
     * Initializes the VFS and mounts rootFS to /
     **/
    void Init(FileSystem* rootFS);

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
     * Creates a symlink to another file
     **/
    int64 CreateSymLink(User* user, const char* path, const Permissions& permissions, const char* linkPath);

    int64 CreateHardLink(User* user, const char* path, const Permissions& permissions, const char* linkPath);

    /**
     * Removes the files directory entry from the containing directory.
     * The node will, however, only be freed if every reference to it is closed.
     **/
    int64 Delete(User* user, const char* path);

    /**
     * Changes the owner of the given file, only the current owner and root can do that
     **/
    int64 ChangeOwner(User* user, const char* path, uint64 newUID, uint64 newGID);
    /**
     * Changes the permissions of the given file, only the current owner and root can do that
     **/
    int64 ChangePermissions(User* user, const char* path, const Permissions& permissions);

    struct NodeStats {
        uint64 nodeID;
        Node::Type type;
        uint64 ownerGID;
        uint64 ownerUID;
        Permissions permissions;

        union {
            uint64 size;
            char linkPath[255];
        };
    };
    int64 Stat(User* user, const char* path, NodeStats& outStats);

    struct ListEntry {
        char name[256];
    };
    int64 List(User* user, const char* path, int& numEntries, ListEntry* entries);

    /**
     * Mount the given FileSystem at the given path.
     * MountPoint has to be an empty folder.
     **/
    int64 Mount(User* user, const char* mountPoint, FileSystem* fs);
    int64 Mount(User* user, const char* mountPoint, const char* fsID);
    int64 Mount(User* user, const char* mountPoint, const char* fsID, const char* devFile);
    int64 Mount(User* user, const char* mountPoint, const char* fsID, uint64 driverID, uint64 devID);

    /**
     * Unmounts the given path
     **/
    int64 Unmount(User* user, const char* mountPoint);

    constexpr uint64 OpenMode_Read = 0x1;           // Open for reading
    constexpr uint64 OpenMode_Write = 0x2;          // Open for writing
    constexpr uint64 OpenMode_Create = 0x100;       // Create file if it doesn't exist
    constexpr uint64 OpenMode_Clear = 0x200;        // Clear file contents if file exists
    constexpr uint64 OpenMode_FailIfExist = 0x400;  // Fail if the file already exists

    /**
     * Opens the given path
     * Returns the FileDescriptor of the opened path, or 0 on error.
     **/
    int64 Open(User* user, const char* path, uint64 mode, uint64& fileDesc);
    /**
     * Closes the given FileDescriptor
     **/
    int64 Close(uint64 desc);
    /**
     * Increments the refCount of the given filedescriptor
     */
    int64 AddRef(uint64 desc);

    /**
     * Reads from the given File and increases the FileDescriptor position by the number of bytes read.
     * This function blocks until at least one byte was read, or returns 0 to indicate eof.
     * @param buffer The buffer to read the data into, should be a Kernel pointer
     * @param bufferSize The maximum number of bytes that fit into the given buffer
     * @returns the number of bytes read, 0 if the end of file was reached.
     **/
    int64 Read(uint64 desc, void* buffer, uint64 bufferSize);
    /**
     * Writes to the given File and increases the FileDescriptor position by the number of bytes written.
     * This function blocks until at least one byte was written, or returns an error code.
     * It should never return 0.
     * @param buffer The buffer to write the data from, should be a Kernel pointer
     * @param bufferSize The number of bytes contained in the given buffer
     * @returns the number of bytes written.
     **/
    int64 Write(uint64 desc, const void* buffer, uint64 bufferSize);

    enum SeekMode {
        SEEK_SET,
        SEEK_REL,
        SEEK_END,
    };
    /**
     * Sets the cursor offset of the given file descriptor to offs, if possible.
     * Returns 0 on success, error otherwise
     **/
    int64 Seek(uint64 desc, SeekMode mode, uint64 offs);

}