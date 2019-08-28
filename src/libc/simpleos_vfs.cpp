#include "simpleos_vfs.h"

#include "syscall.h"

int64 delete_file(const char* path) {
    return syscall_invoke(syscall_delete, (uint64)path);
}

int64 create_file(const char* path) {
    return syscall_invoke(syscall_create_file, (uint64)path);
}
int64 create_folder(const char* path) {
    return syscall_invoke(syscall_create_folder, (uint64)path);
}
int64 create_dev(const char* path, uint64 driverID, uint64 devID) {
    return syscall_invoke(syscall_create_dev, (uint64)path, driverID, devID);
}
int64 create_symlink(const char* path, const char* linkPath) {
    return syscall_invoke(syscall_create_symlink, (uint64)path, (uint64)linkPath);
}
int64 create_hardlink(const char* path, const char* linkPath) {
    return syscall_invoke(syscall_create_hardlink, (uint64)path, (uint64)linkPath);
}

void pipe(int64* readFD, int64* writeFD) {
    syscall_invoke(syscall_pipe, (uint64)readFD, (uint64)writeFD);
}

int64 change_perm(const char* path, uint8 ownerPerm, uint8 groupPerm, uint8 otherPerm) {
    return syscall_invoke(syscall_change_perm, (uint64)path, ownerPerm, groupPerm, otherPerm);
}
int64 change_owner(const char* path, uint64 uid, uint64 gid) {
    return syscall_invoke(syscall_change_owner, (uint64)path, uid, gid);
}

int64 open(const char* path, uint64 mode) {
    return syscall_invoke(syscall_open, (uint64)path, mode);
}
int64 close(int64 fd) {
    return syscall_invoke(syscall_close, fd);
}

/*int64 reopenfd(int64 fd, const char* path, uint64 mode) {
    return syscall_invoke(syscall_reopenfd, fd, (uint64)path, mode);
}*/
int64 copyfd(int64 destFD, int64 srcFD) {
    return syscall_invoke(syscall_copyfd, destFD, srcFD);
}
int64 seekfd(int64 fd, uint64 pos) {
    return syscall_invoke(syscall_seek, fd, pos);
}

int64 read(int64 fd, void* buffer, uint64 bufferSize) {
    return syscall_invoke(syscall_read, fd, (uint64)buffer, bufferSize);
}
int64 write(int64 fd, const void* buffer, uint64 bufferSize) {
    return syscall_invoke(syscall_write, fd, (uint64)buffer, bufferSize);
}

int64 mount(const char* mountPoint, const char* fsID) {
    return syscall_invoke(syscall_mount, (uint64)mountPoint, (uint64)fsID);
}
int64 mount(const char* mountPoint, const char* fsID, const char* dev) {
    return syscall_invoke(syscall_mount_dev, (uint64)mountPoint, (uint64)fsID, (uint64)dev);
}

int64 unmount(const char* mountPoint) {
    return syscall_invoke(syscall_unmount, (uint64)mountPoint);
}