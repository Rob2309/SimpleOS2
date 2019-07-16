#pragma once

#include "DeviceDriver.h"
#include "ktl/vector.h"
#include "terminal/terminal.h"

class VConsoleDriver : public CharDeviceDriver {
public:
    static void Init();

    uint64 AddConsole(Terminal::TerminalInfo* info);

    uint64 Read(uint64 subID, void* buffer, uint64 bufferSize) override;
    uint64 Write(uint64 subID, const void* buffer, uint64 bufferSize) override;

private:
    ktl::vector<Terminal::TerminalInfo*> m_Consoles;
};