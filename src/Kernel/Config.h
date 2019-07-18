#pragma once

#include "devices/RamDeviceDriver.h"
#include "devices/PseudoDeviceDriver.h"
#include "devices/VConsoleDriver.h"

#include "fs/ext2/ext2.h"

#include "exec/elf/ELF.h"

typedef void (*InitFunc)();

/**
 * These functions will be called when the kernel is ready to initialize the device drivers.
 * The IDs of the drivers will be the same as their index in this array 
 **/
static InitFunc config_DeviceDriverInitFuncs[] = {
    &PseudoDeviceDriver::Init,
    &RamDeviceDriver::Init,
    &VConsoleDriver::Init,
};

/**
 * These functions will be called when the kernel is ready to initialize the FileSystem drivers.
 * At least the FileSystem driver of the boot file system needs to be loaded.
 * If config_BootFS_MountToRoot == false, the TempFS driver also needs to be loaded
 **/
static InitFunc config_FSDriverInitFuncs[] = {
    &TempFS::Init,
    &Ext2::Ext2Driver::Init,
};

/**
 * These functions will be called when the kernel is ready to initialize the file execution handlers.
 * At least the execution handler responsible for starting the init command has to be loaded
 **/
static InitFunc config_ExecInitFuncs[] = {
    &ELFExecHandler::Init,
};

// The driver ID of the RamDeviceDriver (only needed when the kernel is started with a Ramdisk)
constexpr uint64 config_RamDeviceDriverID = 1;
// The driver ID of the VConsoleDriver (needed for accessing the on-screen console)
constexpr uint64 config_VConDeviceDriverID = 2;

// If true, the boot filesystem will be directly mounted to /, otherwise it will be mounted into /boot
constexpr bool config_BootFS_MountToRoot = false;
// The driver ID of the device the boot filesystem is located on
constexpr uint64 config_BootFS_DriverID = 1;
// The device ID of the device the boot filesystem is located on
constexpr uint64 config_BootFS_DevID = 0;
// The filesystem type of the boot filesystem
constexpr const char* config_BootFS_FSID = "ext2";

// The command that will be executed after boot to start the first process
constexpr const char* config_Init_Command = "/boot/Init.elf";