#include "InitInvoke.h"

#include "types.h"

#include "klib/stdio.h"

extern "C" int KINIT_ARRAY_START;
extern "C" int KINIT_ARRAY_END;

static const char* g_StageNames[] = {
    nullptr,
    "DevDrivers",
    "FsDrivers",
    "ExecHandlers",
};

void CallInitFuncs(int stage) {
    klog_info("Init", "Calling init funcs for stage %s", g_StageNames[stage]);

    uint64* arr = (uint64*)&KINIT_ARRAY_START;
    uint64* arrEnd = (uint64*)&KINIT_ARRAY_END;
    for(; arr != arrEnd; arr += 2) {
        void (*func)() = (void(*)())arr[0];
        uint64 s = arr[1];
        if(stage == s) {
            func();
        }
    }
}