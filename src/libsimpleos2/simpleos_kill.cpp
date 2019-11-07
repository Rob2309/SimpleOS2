#include "simpleos_kill.h"
#include "syscall.h"

static char g_KillHandlerStack[4096];
static KillHandler g_KillHandler = nullptr;

static void MasterKillHandler() {
    g_KillHandler();
    syscall_invoke(syscall_finish_kill);
}

void setkillhandler(KillHandler handler) {
    if(handler == nullptr) {
        syscall_invoke(syscall_handle_kill, 0, 0);
    } else {
        g_KillHandler = handler;
        syscall_invoke(syscall_handle_kill, (uint64)&MasterKillHandler, (uint64)g_KillHandlerStack + sizeof(g_KillHandlerStack));
    }
}