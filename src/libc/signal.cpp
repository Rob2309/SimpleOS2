#include "signal.h"

static SignalHandler g_Handlers[] = {
    SIG_DFL,
    SIG_DFL,
    SIG_DFL,
    SIG_DFL,
    SIG_DFL,
    SIG_DFL,
};

static void SignalDefault(int) { }

SignalHandler SIG_DFL = SignalDefault;
SignalHandler SIG_IGN = SignalDefault;

SignalHandler signal(int sig, SignalHandler handler) {
    if(sig < 0 || sig >= 6)
        return SIG_ERR;

    SignalHandler prev = g_Handlers[sig];
    g_Handlers[sig] = handler;
    return prev;
}

int raise(int sig) {
    if(sig < 0 || sig >= 6)
        return -1;

    g_Handlers[sig](sig);
    return 0;
}