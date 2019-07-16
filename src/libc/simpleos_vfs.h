#pragma once

#include "types.h"
#include "errno.h"

int64 delete_file(const char* path);

int64 create_file(const char* path);
int64 create_folder(const char* path);
int64 create_dev(const char* path, uint64 driverID, uint64 devID);

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

int64 reopenfd(int64 fd, const char* path, uint64 mode);
int64 copyfd(int64 destFD, int64 srcFD);
int64 seekfd(int64 fd, uint64 pos);

int64 read(int64 fd, void* buffer, uint64 bufferSize);
int64 write(int64 fd, const void* buffer, uint64 bufferSize);

int64 mount(const char* mountPoint, const char* fsID);
int64 mount(const char* mountPoint, const char* fsID, const char* dev);