#include "simpleos_process.h"

#include "syscall.h"

uint64 gettid() {
    return syscall_invoke(syscall_gettid);
}

void thread_waitms(uint64 ms) {
    syscall_invoke(syscall_wait, ms);
}

void thread_create(void (*entry)(void*), void* stack, void* arg) {
    syscall_invoke(syscall_thread_create, (uint64)entry, (uint64)stack, (uint64)arg);
}
void thread_exit(uint64 code) {
    syscall_invoke(syscall_exit, code);
}
int64 thread_movecore(uint64 coreID) {
    syscall_invoke(syscall_move_core, coreID);
}

uint64 fork() {
    return syscall_invoke(syscall_fork);
}
void exec(const char* path) {
    syscall_invoke(syscall_exec, (uint64)path);
}

void setfsbase(uint64 val) {
    syscall_invoke(syscall_setfs, val);
}

int64 detach(int64 tid) {
    return syscall_invoke(syscall_detach, tid);
}
int64 join(int64 tid) {
    return syscall_invoke(syscall_join, tid);
}

void whoami(char* buffer) {
    syscall_invoke(syscall_whoami, (uint64)buffer);
}