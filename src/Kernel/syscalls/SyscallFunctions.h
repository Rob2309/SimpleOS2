#pragma once

#include "types.h"

// This file contains all system calls defined by the kernel

constexpr uint64 syscall_gettid = 1;
constexpr uint64 syscall_wait = 2;
constexpr uint64 syscall_exit = 3;
constexpr uint64 syscall_fork = 4;
constexpr uint64 syscall_exec = 5;
constexpr uint64 syscall_thread_create = 6;
constexpr uint64 syscall_print = 7;
constexpr uint64 syscall_detach = 8;
constexpr uint64 syscall_join = 9;
constexpr uint64 syscall_whoami = 10;
constexpr uint64 syscall_try_join = 11;
constexpr uint64 syscall_kill = 12;
constexpr uint64 syscall_statl = 13;

constexpr uint64 syscall_create_file = 50;
constexpr uint64 syscall_create_folder = 51;
constexpr uint64 syscall_create_dev = 52;
constexpr uint64 syscall_pipe = 53;
constexpr uint64 syscall_delete = 54;
constexpr uint64 syscall_open = 55;
constexpr uint64 syscall_close = 56;
constexpr uint64 syscall_list = 57;
constexpr uint64 syscall_read = 58;
constexpr uint64 syscall_write = 59;
constexpr uint64 syscall_mount = 60;
constexpr uint64 syscall_mount_dev = 61;
constexpr uint64 syscall_change_owner = 62;
constexpr uint64 syscall_change_perm = 63;
constexpr uint64 syscall_copyfd = 64;
constexpr uint64 syscall_reopenfd = 65;
constexpr uint64 syscall_seek = 66;
constexpr uint64 syscall_create_symlink = 67;
constexpr uint64 syscall_create_hardlink = 68;
constexpr uint64 syscall_unmount = 69;
constexpr uint64 syscall_stat = 70;

constexpr uint64 syscall_alloc = 100;
constexpr uint64 syscall_free = 101;

constexpr uint64 syscall_move_core = 500;

constexpr uint64 syscall_setfs = 600;