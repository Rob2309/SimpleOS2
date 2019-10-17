#include "VConsoleDriver.h"

#include "klib/memory.h"
#include "errno.h"

#include "init/Init.h"

#include "devices/DevFS.h"

#include "ktl/vector.h"

struct ConsoleInfo {
    Terminal::TerminalInfo* tInfo;
    int64 foregroundTid;
};
static ktl::vector<ConsoleInfo> g_Consoles;

static void Init() {
    VConsoleDriver* driver = new VConsoleDriver();
}
REGISTER_INIT_FUNC(Init, INIT_STAGE_DEVDRIVERS);

VConsoleDriver::VConsoleDriver()
    : CharDeviceDriver("vconsole")
{ }

uint64 VConsoleDriver::AddConsole(Terminal::TerminalInfo* info) {
    uint64 res = g_Consoles.size();
    g_Consoles.push_back({ info, 0 });

    DevFS::RegisterCharDevice("tty0", GetDriverID(), res);

    return res;
}

int64 VConsoleDriver::DeviceCommand(uint64 subID, int64 command, void* arg) {
    return OK;
}

uint64 VConsoleDriver::Read(uint64 subID, void* buffer, uint64 bufferSize) {
    return 0;
}
uint64 VConsoleDriver::Write(uint64 subID, const void* buffer, uint64 bufferSize) {
    if(subID >= g_Consoles.size())
        return ErrorInvalidDevice;

    auto& cInfo = g_Consoles[subID];
    auto tInfo = cInfo.tInfo;

    Terminal::Begin();

    const char* str = (const char*)buffer;
    for(uint64 i = 0; i < bufferSize; i++) {
        char c = str[i];
        if(c == '\n') {
            Terminal::NewLine(tInfo);
        } else if(c == '\b') {
            Terminal::RemoveChar(tInfo);
        } else if(c == '\t') {
            Terminal::PutChar(tInfo, ' ');
            Terminal::PutChar(tInfo, ' ');
            Terminal::PutChar(tInfo, ' ');
            Terminal::PutChar(tInfo, ' ');
        } else {
            Terminal::PutChar(tInfo, c);
        }
    }

    Terminal::End();

    return bufferSize;
}