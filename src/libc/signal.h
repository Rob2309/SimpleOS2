#pragma once

#include "types.h"

typedef uint64 sig_atomic_t;

#define SIGABRT 0
#define SIGFPE 1
#define SIGILL 2
#define SIGINT 3
#define SIGSEGV 4
#define SIGTERM 5

typedef void (*SignalHandler)(int);

extern SignalHandler SIG_DFL;
extern SignalHandler SIG_IGN;
#define SIG_ERR nullptr

SignalHandler signal(int sig, SignalHandler handler);
int raise(int sig);