#pragma once

#include "DeviceDriver.h"
#include "terminal/terminal.h"

#include <vector>

class VConsoleDriver : public CharDeviceDriver {
public:
    static constexpr int64 COMMAND_SET_FOREGROUND = 1;

public:
    VConsoleDriver();

    uint64 AddConsole(Terminal::TerminalInfo* info);

    int64 DeviceCommand(uint64 subID, int64 command, void* arg) override;

    uint64 Read(uint64 subID, void* buffer, uint64 bufferSize) override;
    uint64 Write(uint64 subID, const void* buffer, uint64 bufferSize) override;
};