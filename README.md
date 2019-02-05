# SimpleOS2

This is a very simple x86_64 OS I am creating for learning purposes.

## Implemented features:
- Simple UEFI Bootloader that loads modules listed in a configuration file (currently supports ELF and IMG files)
- Basic physical memory management
- Terminal that can print text (no input functionality yet)
- Basic Interrupt handling

## Planned features:
- HDD support (SATA)
- Color support for terminal
- virtual memory management
- full APIC support
- Multitasking / Multi Core support
- Process and Thread management
- USB support (maybe)

## Specs
This OS should basically run on any x86_64 machine that supports UEFI. If you find a machine that does not work with SimpleOS2, please contact me.

## Building SimpleOS2
### Prerequisites
- Some kind of Linux environment (or WSL)
- Any C/C++ compiler that can build elf64 binaries
- Any C/C++ compiler that can build pe64 binaries
- NASM
### Build commands
- make partition.img: build a raw partition image that contains the OS
- make partition.vdi: build a VirtualBox disk image that contains the OS (Requires VirtualBox installation)

## Emulating SimpleOS2
- make run: run the OS in qemu (requires qemu-system-i86_64)
- make runvbox: run the OS in VirtualBox (Requires a VirtualBox machine with name 'SimpleOS2')
- make clean: remove everything but the sources

## Running on real Hardware
I have run SimpleOS2 several times on my own hardware and never encountered any damage. Nevertheless, I do not take any responsibility for any kind of damage to your system!
