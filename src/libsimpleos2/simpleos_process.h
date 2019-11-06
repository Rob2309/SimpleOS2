#pragma once

#include "simpleos_types.h"

/**
 * Returns the Thread ID of the calling thread.
 **/
uint64 gettid();

/**
 * Stops this thread for at least ms milliseconds.
 **/
void thread_waitms(uint64 ms);

/**
 * Creates a new thread in the current memory space.
 * @param entry         The entry point for the new thread
 * @param stack         The top of the stack for the new thread
 * @param arg           An optional argument to pass to the threads entry point
 * @note                This function should not be used directly as thread local storage will not work. Use CreateThread from simpleos_thread.h instead
 **/
void thread_create(void (*entry)(void*), void* stack, void* arg);
/**
 * Exits the calling thread and returns an exit code to the parent thread.
 * @param code          The exit code to return to the parent thread.
 *                      A zero value denoted successful execution.
 **/
void thread_exit(uint64 code);
/**
 * @deprecated should not be used, as this function is only for testing purposes
 * Moves the calling thread onto another processor core.
 **/
int64 thread_movecore(uint64 coreID);

/**
 * Creates an exact copy of the calling thread.
 * @returns         0 to the new thread
 *                  the tid of the new thread to the parent
 **/
uint64 fork();
/**
 * Replaces the calling threads program with the program denoted by path.
 * @param argc, argv        The arguments to pass to the new program's main function
 **/
void exec(const char* path, int argc, const char* const* argv);

/**
 * Sets the value of the FS processor register.
 * @note This register is used for thread local storage, only use this function if you know what you are doing.
 **/
void setfsbase(uint64 val);

/**
 * Detaches a child thread from the calling thread.
 * The detached thread will no longer be killed when this thread exits.
 **/
int64 detach(int64 tid);
/**
 * Waits for a child thread to finish and returns its exit code.
 **/
int64 join(int64 tid);
/**
 * Checks if a child process has exited, but will not block if it has not exited yet.
 * @returns 1. ErrorThreadNotExited when the specified thread has not exited yet
 *          2. the threads exit code otherwise
 **/
int64 try_join(int64 tid);
/**
 * Terminates the specified thread without waiting for it.
 * A thread can kill itself.
 **/
int64 kill(int64 tid);

/**
 * Returns the username of the user that owns this thread.
 **/
void whoami(char* buffer);

int64 changeuser(uint64 uid, uint64 gid);