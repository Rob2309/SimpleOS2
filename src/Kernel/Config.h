#pragma once

#include "devices/RamDeviceDriver.h"
#include "devices/PseudoDeviceDriver.h"

#include "fs/ext2/ext2.h"

#include "exec/elf/ELF.h"

typedef void (*InitFunc)();

static InitFunc config_DeviceDriverInitFuncs[] = {
    &PseudoDeviceDriver::Init,
    &RamDeviceDriver::Init,
};

static InitFunc config_FSDriverInitFuncs[] = {
    &TempFS::Init,
    &Ext2::Ext2Driver::Init,
};

static InitFunc config_ExecInitFuncs[] = {
    &ELFExecHandler::Init,
};

constexpr uint64 config_RamDeviceDriverID = 1;

constexpr bool config_BootFS_MountToRoot = false;
constexpr uint64 config_BootFS_DriverID = 1;
constexpr uint64 config_BootFS_DevID = 0;
constexpr const char* config_BootFS_FSID = "ext2";

constexpr const char* config_Init_Command = "/boot/Init.elf";