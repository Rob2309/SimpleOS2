#include "VConsoleDriver.h"

#include "klib/memory.h"
#include "errno.h"

#include "init/Init.h"

#include "devices/DevFS.h"

#include "ktl/vector.h"

#include "scheduler/Scheduler.h"
#include "devices/ps2/PS2Keys.h"

#include "klib/stdio.h"

struct ConsoleInfo {
    Terminal::TerminalInfo* tInfo;
    int64 foregroundTid;

    StickyLock inBufferLock;
    int64 inBufferReadPos;
    int64 inBufferWritePos;
    char inBuffer[256];

    bool leftShift;
    bool rightShift;
    bool leftCtrl;
    bool rightCtrl;
};
static ktl::vector<ConsoleInfo> g_Consoles;

static void WriteChar(char c) {
    auto& con = g_Consoles[0];

    con.inBufferLock.Spinlock();
    if(con.inBufferWritePos - con.inBufferReadPos < sizeof(con.inBuffer)) {
        con.inBuffer[con.inBufferWritePos++ % sizeof(con.inBuffer)] = c;
    }
    con.inBufferLock.Unlock();
}

static int64 VConsoleThread(uint64, uint64) {
    klog_info("VCon", "VCon 0 thread starting");

    auto& con = g_Consoles[0];

    con.inBufferReadPos = 0;
    con.inBufferWritePos = 0;
    con.leftShift = false;
    con.rightShift = false;
    con.leftCtrl = false;
    con.rightCtrl = false;

    uint64 desc;
    int64 error = VFS::Open("/dev/keyboard", VFS::OpenMode_Read, desc);
    if(error != OK) {
        klog_error("VCon", "Failed to open Keyboard file: %s", ErrorToString(error));
        return -1;
    }

    while(true) {
        PS2::Key key;
        int64 count = VFS::Read(desc, &key, sizeof(key));
        if(count == 0)
            continue;

        if(key == PS2::KEY_SHIFT_LEFT)
            con.leftShift = true;
        else if(key == (PS2::KEY_SHIFT_LEFT | PS2::KEY_RELEASED))
            con.leftShift = false;
        else if(key == PS2::KEY_SHIFT_RIGHT)
            con.rightShift = true;
        else if(key == (PS2::KEY_SHIFT_RIGHT | PS2::KEY_RELEASED))
            con.rightShift = false;
        else if(key == PS2::KEY_CTRL_LEFT)
            con.leftCtrl = true;
        else if(key == (PS2::KEY_CTRL_LEFT | PS2::KEY_RELEASED))
            con.leftCtrl = false;
        else if(key == PS2::KEY_CTRL_RIGHT)
            con.rightCtrl = true;
        else if(key == (PS2::KEY_CTRL_RIGHT | PS2::KEY_RELEASED))
            con.rightCtrl = false;
        else if(key == PS2::KEY_UP) {
            WriteChar('\1');
        } else if(key == PS2::KEY_DOWN) {
            WriteChar('\2');
        } else if(con.leftCtrl && key == PS2::KEY_C) {
            if(con.foregroundTid != 0) {
                Scheduler::ThreadKill(con.foregroundTid);
                con.foregroundTid = 0;
            }
        } else {
            if(key & PS2::KEY_RELEASED)
                continue;

            if(con.leftShift || con.rightShift) {
                if(key >= PS2::KEY_A && key <= PS2::KEY_Z)
                    WriteChar('A' + (key - PS2::KEY_A));
                else {
                    switch(key) {
                    case PS2::KEY_0: WriteChar('='); break;
                    case PS2::KEY_1: WriteChar('!'); break;
                    case PS2::KEY_2: WriteChar('"'); break;
                    case PS2::KEY_4: WriteChar('$'); break;
                    case PS2::KEY_5: WriteChar('%'); break;
                    case PS2::KEY_6: WriteChar('&'); break;
                    case PS2::KEY_7: WriteChar('/'); break;
                    case PS2::KEY_8: WriteChar('('); break;
                    case PS2::KEY_9: WriteChar(')'); break;

                    case PS2::KEY_QUOTE: WriteChar('`'); break;
                    case PS2::KEY_BACKSPACE: WriteChar('\b'); break;
                    case PS2::KEY_TAB: WriteChar('\t'); break;
                    case PS2::KEY_ENTER: WriteChar('\n'); break;
                    case PS2::KEY_PLUS: WriteChar('*'); break;
                    case PS2::KEY_MINUS: WriteChar('_'); break;
                    case PS2::KEY_COMMA: WriteChar(';'); break;
                    case PS2::KEY_PERIOD: WriteChar(':'); break;
                    case PS2::KEY_SPACE: WriteChar(' '); break;
                    }
                }
            } else if(con.leftCtrl || con.rightCtrl) {

            } else {
                if(key >= PS2::KEY_0 && key <= PS2::KEY_9)
                    WriteChar('0' + (key - PS2::KEY_0));
                else if(key >= PS2::KEY_A && key <= PS2::KEY_Z)
                    WriteChar('a' + (key - PS2::KEY_A));
                else {
                    switch(key) {
                    case PS2::KEY_QUOTE: WriteChar('\''); break;
                    case PS2::KEY_BACKSPACE: WriteChar('\b'); break;
                    case PS2::KEY_TAB: WriteChar('\t'); break;
                    case PS2::KEY_ENTER: WriteChar('\n'); break;
                    case PS2::KEY_PLUS: WriteChar('+'); break;
                    case PS2::KEY_MINUS: WriteChar('-'); break;
                    case PS2::KEY_COMMA: WriteChar(','); break;
                    case PS2::KEY_PERIOD: WriteChar('.'); break;
                    case PS2::KEY_SPACE: WriteChar(' '); break;
                    }
                }
            }
        }
    }
}

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
    Scheduler::CreateKernelThread(VConsoleThread);

    return res;
}

int64 VConsoleDriver::DeviceCommand(uint64 subID, int64 command, void* arg) {
    if(subID >= g_Consoles.size())
        return ErrorInvalidDevice;

    auto& con = g_Consoles[subID];

    if(command == COMMAND_SET_FOREGROUND) {
        con.foregroundTid = (int64)arg;
    }

    return OK;
}

uint64 VConsoleDriver::Read(uint64 subID, void* buffer, uint64 bufferSize) {
    if(subID >= g_Consoles.size())
        return ErrorInvalidDevice;

    auto& cInfo = g_Consoles[subID];

    cInfo.inBufferLock.Spinlock();
    int64 rem = cInfo.inBufferWritePos - cInfo.inBufferReadPos;
    if(rem > bufferSize)
        rem = bufferSize;
    for(int i = 0; i < rem; i++)
        ((char*)buffer)[i] = cInfo.inBuffer[(cInfo.inBufferReadPos + i) % sizeof(cInfo.inBuffer)];
    cInfo.inBufferReadPos += rem;
    cInfo.inBufferLock.Unlock();

    return rem;
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