#pragma once

#include "simpleos_types.h"

uint64 gettid();

void thread_waitms(uint64 ms);

void thread_create(void (*entry)(void*), void* stack, void* arg);
void thread_exit(uint64 code);
int64 thread_movecore(uint64 coreID);

uint64 fork();
void exec(const char* path, int argc, const char* const* argv);

void setfsbase(uint64 val);

int64 detach(int64 tid);
int64 join(int64 tid);
int64 try_join(int64 tid);
int64 kill(int64 tid);

void whoami(char* buffer);