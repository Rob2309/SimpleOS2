#pragma once

#include "simpleos_types.h"

/**
 * Deletes the file denoted by path.
 * @returns     - OK on success
 *              - an error code on failure
 * @note any open file descriptors referring to this file will still be valid. The file will only be deleted once every file descriptor is closed.
 **/
int64 delete_file(const char* path);

/**
 * Creates a normal file at the given path.
 **/
int64 create_file(const char* path);
/**
 * Creates an empty folder at the given path.
 **/
int64 create_folder(const char* path);
/**
 * Creates a device file at the given path.
 **/
int64 create_dev(const char* path, uint64 driverID, uint64 devID);
/**
 * Creates a symbolic link named path linking to linkPath.
 **/
int64 create_symlink(const char* path, const char* linkPath);
/**
 * Creates a hardlink named path linking to the file denoted by linkPath.
 **/
int64 create_hardlink(const char* path, const char* linkPath);

/**
 * Creates a uni directional pipe.
 * @param readFD        Pointer to a variable that will hold a file descriptor to read from the pipe.
 * @param writeFD       Pointer to a variable that will hold a file descriptor to write to the pipe.
 **/
void pipe(int64* readFD, int64* writeFD);

int64 change_perm(const char* path, uint8 ownerPerm, uint8 groupPerm, uint8 otherPerm);
int64 change_owner(const char* path, uint64 uid, uint64 gid);

constexpr uint64 open_mode_read = 0x1;
constexpr uint64 open_mode_write = 0x2;
constexpr uint64 open_mode_create = 0x100;
constexpr uint64 open_mode_clear = 0x200;
constexpr uint64 open_mode_failexist = 0x400;
int64 open(const char* path, uint64 mode);
int64 close(int64 fd);

int64 copyfd(int64 destFD, int64 srcFD);

constexpr int64 seek_mode_set = 0;
constexpr int64 seek_mode_rel = 1;
constexpr int64 seek_mode_end = 2;
int64 seekfd(int64 fd, int64 mode, uint64 pos);

int64 read(int64 fd, void* buffer, uint64 bufferSize);
int64 write(int64 fd, const void* buffer, uint64 bufferSize);

int64 devcmd(int64 fd, int64 cmd, void* arg);

int64 mount(const char* mountPoint, const char* fsID);
int64 mount(const char* mountPoint, const char* fsID, const char* dev);

int64 unmount(const char* mountPoint);

struct ListEntry {
    char name[256];
};
/**
 * Lists the directory entries of a directory denoted by path.
 * @param numEntries        - on input the number of entries that the given buffer can hold
 *                          - on output the number of entries that the directory contians
 * @param entries           The buffer to write the entries to
 **/
int64 list(const char* path, int* numEntries, ListEntry* entries);

enum NodeType {
    NODE_FILE,
    NODE_DIRECTORY,
    NODE_DEVICE_CHAR,
    NODE_DEVICE_BLOCK,
    NODE_PIPE,
    NODE_SYMLINK,
};
constexpr uint8 PermRead = 0x1;
constexpr uint8 PermWrite = 0x2;
struct Permissions {
    uint8 owner;
    uint8 group;
    uint8 other;
};
struct Stats {
    uint64 nodeID;
    NodeType type;
    uint64 ownerGID;
    uint64 ownerUID;
    Permissions perms;
    
    union {
        uint64 size;
        char linkPath[255];
    };
};
/**
 * Returns information about the file at path.
 * @note if path points to a symlink, stat will return information about the file that the symlink points to.
 **/
int64 stat(const char* path, Stats* stats);
/**
 * Returns information about the file at path.
 * @note if path points to a symlink, stat will return information about the symlink.
 **/
int64 statl(const char* path, Stats* stats);

/**
 * Changes the working directory of the calling thread.
 **/
int64 changedir(const char* path);
/**
 * Writes the calling thread's working directory into pathBuffer.
 **/
int64 pwd(char* pathBuffer);

constexpr int64 stdinfd = 0;
constexpr int64 stdoutfd = 1;
constexpr int64 stderrfd = 2;