#pragma once

#include "types.h"

uint64 getpid();
uint64 gettid();

void thread_waitms(uint64 ms);

void thread_create(void (*entry)(void*), void* stack, void* arg);
void thread_exit(uint64 code);
int64 thread_movecore(uint64 coreID);

uint64 fork();
void exec(const char* path);

void setfsbase(uint64 val);