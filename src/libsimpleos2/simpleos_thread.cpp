#include "simpleos_thread.h"

#include "simpleos_alloc.h"
#include "simpleos_process.h"

ELFProgramInfo* g_ProgInfo;

static void RunThread(void* arg) {
    if(g_ProgInfo->masterTLSSize != 0) {
        uint64 allocSize = g_ProgInfo->masterTLSSize + sizeof(ELFThread);
        char* alloc = (char*)malloc(allocSize);

        for(int i = 0; i < g_ProgInfo->masterTLSSize; i++)
            alloc[i] = g_ProgInfo->masterTLSAddress[i];

        ELFThread* thread = (ELFThread*)&alloc[g_ProgInfo->masterTLSSize];
        thread->allocPtr = alloc;
        thread->selfPtr = thread;
        thread->progInfo = g_ProgInfo;

        setfsbase((uint64)thread); 
    }

    int (*func)() = (int (*)())arg;

    int res = func();
    thread_exit(res);
}

void CreateThread(int (*func)()) {
    char* stack = (char*)malloc(16 * 4096) + 16 * 4096;
    thread_create(&RunThread, stack, (void*)func);
}