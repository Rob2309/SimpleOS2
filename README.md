# SimpleOS2

This is a very simple x86_64 OS I am creating for learning purposes.

## Implemented features:
- Simple UEFI Bootloader (to be replaced by GRUB in the future?)
- Physical and Virtual memory management
- Terminal that can print 32-Bit colored text (no input functionality yet)
- Interrupt handling
- APIC Timer support
- IOAPIC support
- User mode processes
- Kernel Threads
- Multitasking & Multithreading
- System calls via syscall instruction (e.g. fork & exec)
- Linux-like virtual file system (with mount support)
- FileSystem driver API
- Character- and BlockDevice driver API
- Multi-Core support
- Usermode SSE & FPU support
- Basic shell that can run programs with command line arguments
- Basic ACPI support using ACPICA (currently only shutdown supported)
- Basic readonly Ext2 driver (used for ramdisk)
- Subset of standard c library supported

## Planned features:
- HDD support (SATA)
- USB support (madness)
- IP Stack
- Full C library support

## Specs
This OS should basically run on any x86_64 machine that supports UEFI. If you find a machine that does not work with SimpleOS2, please contact me.

## Building SimpleOS2
### Prerequisites
- Some kind of Linux environment (or WSL)
- Standard 64-Bit Elf GCC
- Standard 64-Bit mingw GCC
- NASM
- mtools
- debugfs
- parted
### Build commands
- ``make depcheck``:            check if all dependencies are installed
- ``make partition``:           build a raw partition image that contains the OS
- ``make disk``:                build a raw disk image that contains the OS
- ``make diskvbox``:            build a virtualbox disk image (.vdi) that contains the OS
- ``make clean``:               remove everything but the sources

## Emulating SimpleOS2
- ``make runkvm``:              run the OS in qemu (requires qemu-system-i86_64)
- ``make runvbox``:             run the OS in VirtualBox (Requires a VirtualBox machine with name 'SimpleOS2' and partition.vdi as hard disk, chipset has to be ICH9)
- ``make debug``:               run the OS in qemu and debug it in GDB

## Configurations
- every command above can be followed by ``config=Release`` to use/build a Release configuration of the OS

## Running on real Hardware
I have run SimpleOS2 on my own hardware several times and never encountered any damage. Nevertheless, I do not take any responsibility for any kind of damage to your system!

## Modifying SimpleOS2
[DEVELOPMENT.md](DEVELOPMENT.md) contains information for developers who want to have fun with SimpleOS2

## Lessons learned
- When your Code works on Qemu but triple faults on VirtualBox, you have missed some very important minor detail in the AMD64 spec
- When your Code works on VirtualBox but triple faults on Qemu, chances are Qemu does not recognize that you've written to the KernelGSBase MSR (you have to write 0 to gs and fs)
- When your SMP Code crashes when booting more that two Processor Cores, they are probably booting on the same stack (took 2 days to find this stupid bug)
- *NEVER* *EVER* forget that stacks grow down. I spent multiple days searching seemingly random bugs, just to notice that I allocated a memory area for some stack and forgot that the stack should begin at the **end** of the allocated space
