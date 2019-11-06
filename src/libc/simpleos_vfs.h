#pragma once

#include "simpleos_types.h"

int64 delete_file(const char* path);

int64 create_file(const char* path);
int64 create_folder(const char* path);
int64 create_dev(const char* path, uint64 driverID, uint64 devID);
int64 create_symlink(const char* path, const char* linkPath);
int64 create_hardlink(const char* path, const char* linkPath);

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

//int64 reopenfd(int64 fd, const char* path, uint64 mode);
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
int64 stat(const char* path, Stats* stats);
int64 statl(const char* path, Stats* stats);

int64 changedir(const char* path);
int64 pwd(char* pathBuffer);

constexpr int64 stdinfd = 0;
constexpr int64 stdoutfd = 1;
constexpr int64 stderrfd = 2;