
This file is intended for Developers who are interested in modifying the OS or just to better understand the structure of the Code.

## Bootloader
The bootloader is currently just a simple UEFI program that loads a few files from disk and sets up the main kernel image. Since the kernel image is actually an ELF shared library and not an executable, it can be relocated to any memory address easily.  
The KernelHeader structure (KernelHeader.h) is the main way to convey information of any kind to the kernel.

## Kernel
The Kernel is the heart of the SimpleOS2 project. It is structured in a modular way to allow anyone to extend the kernel easily.
The goal is to create a Kernel that is very similar to the POSIX standard, however, the kernel is in no way POSIX compliant. The aim is just to create a Kernel that takes the good aspects of e.g. linux and builds upon them.

### General Structure
Folder | Purpose
-------|----------
[acpi](src/Kernel/acpi) | Everything related to the ACPI standard, including the OS specific layer of the ACPICA system.
[arch](src/Kernel/arch) | Everything that is very specific to the x64 architecture (mostly initialization code). Admittedly, much more should be in this folder, as large amounts of architecture dependent code is in other folders, will be resolved in the future.
[atomic](src/Kernel/atomic) | Definition of basic atomic types required by the Kernel.
[devices](src/Kernel/devices) | All standard device drivers are located in this folder. For the definition of the Device Driver API, see DeviceDriver.h
[exec](src/Kernel/exec) | All the file execution handlers are located in this folder (currently only the ELF format is supported)
[fs](src/Kernel/fs) | Everything related to the Virtual File System is located in this folder, as well as the standard filesystem drivers (currently only ext2)
[init](src/Kernel/init) | Helpers to register Init functions in the Kernel
[interrupts](src/Kernel/interrupts) | The interrupt handling mechanisms
[klib](src/Kernel/klib) | Every utility function that is remotely comparable to a C Standard library function is in this folder
[ktl](src/Kernel/ktl) | Kernel Template Library. Anything that is comparable to a C++ STL header is placed here
[locks](src/Kernel/locks) | All types of locks (StickyLock, QueueLock, ...)
[memory](src/Kernel/memory) | The MemoryManager and KernelHeap manager
[multicore](src/Kernel/multicore) | The startup code for SMP cores
[percpu](src/Kernel/percpu) | Helper code to declare variables that each processor core has an own copy of (comparable to thread-local storage in usermode)
[scheduler](src/Kernel/scheduler) | The Process/Thread Scheduler code and struct definitions
[syscalls](src/Kernel/syscalls) | System call handling code
[terminal](src/Kernel/terminal) | A very rudimentary terminal that is used by the kernel to draw to the screen
[time](src/Kernel/time) | Basic timekeeping functions

File | Purpose
-----|----------
[Config.h](src/Kernel/Config.h) | This file is used to configure the startup sequence of the Kernel. Here you can specify which device should be mounted to /boot and which file should be executed by the init process.
[errno.h](src/Kernel/errno.h) | Contains the definitions of all error codes used in the kernel
[main.cpp](src/Kernel/main.cpp) | Contains the main function of the kernel that is called directly by the bootloader

### Device Drivers
Device drivers are the main way in which the Kernel communicates with all kinds of (hardware) devices. There are two basic types of Devices, character devices and block devices.

Device Type | Description
--|--
Character Device | A character device is basically any device that does not allow random access. For example, a keyboard driver would be a character device, as the inputs are sent one after the other and cannot be indexed.
Block Device | A Block device is any device that can be accessed in a random order. Often these devices are accessed on a block by block basis instead of byte basis. An example would be a hard drive. It can access any block at any time, in no specific order.

These two types of devices have slightly different APIs exposed to them. To get a feel for the API, take a look at [RamDeviceDriver.cpp](src/Kernel/devices/RamDeviceDriver.cpp) and [VConsoleDriver.cpp](src/Kernel/devices/VConsoleDriver.cpp)

### Filesystem Drivers
Filesystem drivers are used to mount different types of filesystems into one unified Virtual File System. There are two types of Filesystems

FS Type | Description
--|--
Block Device FS | A Filesystem that is backed by an actual block device (e.g. a harddriver or ramdisk).
Virtual FS | A Filesystem that does not use any device and resides entirely in RAM

To get a feel for the API, take a look at the [TempFS driver](src/Kernel/fs/TempFS.cpp) (Virtual FS) and the [readonly ext2 driver](src/Kernel/fs/ext/ext2.cpp) (Block Device FS).

### File Execution Handlers
File Execution Handlers give the kernel a way to support execution of different filetypes. Currently only ELF executables are supported, but by adding other ExecHandlers, theoretically any file could be run by the OS.
To get a feel for the API, take a look at the [ELF ExecHandler](src/Kernel/exec/elf/ELF.cpp)

### Configuring the Kernel
The File [Config.h](src/Kernel/Config.h) contains the main configuration options that the Kernel uses on startup. The most important configuration option is the Boot filesystem type, driver and device ID. Those are used to mount the initial root filesystem on which the Init program should be located.
Further information is located directly in the Configuration file.

## System Calls
System calls are the only way in which a usermode program can invoke code in the kernel. The allocated system call IDs can be found in [SyscallFunctions.h](src/Kernel/syscalls/SyscallFunctions.h). In order to define a new system call, the file [SyscallDefine.h](src/Kernel/syscalls/SyscallDefine.h) can be included. This file defines the helper macros SYSCALL_DEFINE0, SYSCALL_DEFINE1, SYSCALL_DEFINE2, SYSCALL_DEFINE3 and SYSCALL_DEFINE4, which can be used to define a syscall handler with the given number of arguments. The new syscall's ID should be inserted into the [SyscallFunctions.h](src/Kernel/syscalls/SyscallFunctions.h) file and the libc's [syscall.h](src/libc/syscall.h) in order to keep everything synchronized. With all these steps completed, the new system call can be invoked by calling the libc's ``syscall_invoke`` function with the respective syscallID.