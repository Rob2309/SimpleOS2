#include "ELFProgramInfo.h"

#include "simpleos_process.h"
#include "stdlib.h"
#include "thread.h"

#include "simpleos_raw.h"

extern int main(int argc, char** argv);

extern "C" void __start(ELFProgramInfo* info) {
    g_ProgInfo = info;

    if(info->masterTLSSize != 0) {
        uint64 allocSize = info->masterTLSSize + sizeof(ELFThread);
        char* alloc = (char*)malloc(allocSize);

        for(int i = 0; i < info->masterTLSSize; i++)
            alloc[i] = info->masterTLSAddress[i];

        ELFThread* thread = (ELFThread*)&alloc[info->masterTLSSize];
        thread->allocPtr = alloc;
        thread->selfPtr = thread;
        thread->progInfo = info;

        setfsbase((uint64)thread);
    }

    int res = main(g_ProgInfo->argc, g_ProgInfo->argv);
    exit(res);
};