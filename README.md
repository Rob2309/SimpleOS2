# SimpleOS2

This is a very simple x86_64 OS I am creating for learning purposes.

## Implemented features:
- Simple UEFI Bootloader
- Basic Physical and Virtual memory management
- Terminal that can print colored text (no input functionality yet)
- Basic Interrupt handling
- APIC Timer support
- User mode processes
- Multitasking & Basic process management
- Basic System calls via interrupt 0x80
- Process forking and exiting
- Linux-like virtual file system (with basic mounting support)

## Planned features:
- HDD support (SATA)
- full APIC support
- Multi Core support
- Thread management
- USB support (maybe)

## Specs
This OS should basically run on any x86_64 machine that supports UEFI. If you find a machine that does not work with SimpleOS2, please contact me.

## Building SimpleOS2
### Prerequisites
- Some kind of Linux environment (or WSL)
- Standard 64-Bit Elf GCC
- Standard 64-Bit mingw GCC
- NASM
### Build commands
- make partition.img: build a raw partition image that contains the OS
- make partition.vdi: build a VirtualBox disk image that contains the OS (Requires VirtualBox installation)

## Emulating SimpleOS2
- make run: run the OS in qemu (requires qemu-system-i86_64)
- make runvbox: run the OS in VirtualBox (Requires a VirtualBox machine with name 'SimpleOS2' and partition.vdi as hard disk)
- make clean: remove everything but the sources

## Running on real Hardware
I have run SimpleOS2 several times on my own hardware and never encountered any damage. Nevertheless, I do not take any responsibility for any kind of damage to your system!
