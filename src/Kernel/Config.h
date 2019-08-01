#pragma once

#include "devices/RamDeviceDriver.h"
#include "devices/PseudoDeviceDriver.h"
#include "devices/VConsoleDriver.h"

#include "fs/ext2/ext2.h"

#include "exec/elf/ELF.h"

typedef void (*InitFunc)();

constexpr const char* config_HelloMessage = "Welcome to SimpleOS2!";

// The device to use for the boot filesystem
constexpr const char* config_BootFS_DevFile = "/dev/ram0";
// The filesystem type of the boot filesystem
constexpr const char* config_BootFS_FSID = "ext2";

// The command that will be executed after boot to start the first process
constexpr const char* config_Init_Command = "/boot/Init.elf";