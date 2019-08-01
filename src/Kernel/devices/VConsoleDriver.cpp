#include "VConsoleDriver.h"

#include "klib/memory.h"
#include "errno.h"

#include "init/Init.h"

#include "devices/DevFS.h"

static void Init() {
    VConsoleDriver* driver = new VConsoleDriver();
}
REGISTER_INIT_FUNC(Init, INIT_STAGE_DEVDRIVERS);

VConsoleDriver::VConsoleDriver()
    : CharDeviceDriver("vconsole")
{ }

uint64 VConsoleDriver::AddConsole(Terminal::TerminalInfo* info) {
    uint64 res = m_Consoles.size();
    m_Consoles.push_back(info);

    DevFS::RegisterCharDevice("tty0", GetDriverID(), res);

    return res;
}

uint64 VConsoleDriver::Read(uint64 subID, void* buffer, uint64 bufferSize) {
    return 0;
}
uint64 VConsoleDriver::Write(uint64 subID, const void* buffer, uint64 bufferSize) {
    if(subID >= m_Consoles.size())
        return ErrorInvalidDevice;

    Terminal::TerminalInfo* tInfo = m_Consoles[subID];

    const char* str = (const char*)buffer;
    for(uint64 i = 0; i < bufferSize; i++) {
        char c = str[i];
        if(c == '\n') {
            Terminal::NewLine(tInfo);
        } else {
            Terminal::PutChar(tInfo, c);
        }
    }

    return bufferSize;
}