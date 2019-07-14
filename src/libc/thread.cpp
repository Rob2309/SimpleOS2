#include "thread.h"

#include "Syscall.h"
#include "stdlib.h"

ELFProgramInfo* g_ProgInfo;

static void RunThread(int (*func)()) {
    if(g_ProgInfo->masterTLSSize != 0) {
        uint64 allocSize = g_ProgInfo->masterTLSSize + sizeof(ELFThread);
        char* alloc = (char*)malloc(allocSize);

        for(int i = 0; i < g_ProgInfo->masterTLSSize; i++)
            alloc[i] = g_ProgInfo->masterTLSAddress[i];

        ELFThread* thread = (ELFThread*)&alloc[g_ProgInfo->masterTLSSize];
        thread->allocPtr = alloc;
        thread->selfPtr = thread;
        thread->progInfo = g_ProgInfo;

        Syscall::SetFS((uint64)alloc); 
    }

    int res = func();
    Syscall::Exit(res);
}

void CreateThread(int (*func)()) {
    char* stack = (char*)malloc(16 * 4096) + 16 * 4096;
    Syscall::CreateThread((uint64)&RunThread, (uint64)stack, (uint64)func);
}