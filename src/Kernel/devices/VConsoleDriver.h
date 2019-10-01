#pragma once

#include "DeviceDriver.h"
#include "terminal/terminal.h"

#include <vector>

class VConsoleDriver : public CharDeviceDriver {
public:
    VConsoleDriver();

    uint64 AddConsole(Terminal::TerminalInfo* info);

    uint64 Read(uint64 subID, void* buffer, uint64 bufferSize) override;
    uint64 Write(uint64 subID, const void* buffer, uint64 bufferSize) override;

private:
    std::vector<Terminal::TerminalInfo*> m_Consoles;
};