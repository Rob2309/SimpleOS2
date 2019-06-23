# SimpleOS2

This is a very simple x86_64 OS I am creating for learning purposes.

## Implemented features:
- Simple UEFI Bootloader (to be replaced by GRUB in the future?)
- Physical and Virtual memory management
- Terminal that can print 32-Bit colored text (no input functionality yet)
- Interrupt handling
- APIC Timer support
- User mode processes
- Multitasking & Multithreading
- Basic System calls via syscall instruction (e.g. fork & exec)
- Linux-like virtual file system (with basic mounting support)
- Character- and BlockDevice driver API

## Planned features:
- HDD support (SATA)
- Multi Core support
- USB support (maybe)

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
### Build commands
- ``make depcheck``:            check if all dependencies are installed
- ``make partition``:           build a raw partition image that contains the OS
- ``make vboxpartition``:       build a VirtualBox disk image that contains the OS (Requires VirtualBox installation)
- ``make clean``:               remove everything but the sources

## Emulating SimpleOS2
- ``make run``:                 run the OS in qemu (requires qemu-system-i86_64)
- ``make runvbox``:             run the OS in VirtualBox (Requires a VirtualBox machine with name 'SimpleOS2' and partition.vdi as hard disk)

## Configurations
- every command above can be followed by ``config=Release`` to build a Release configuration of the OS

## Running on real Hardware
I have run SimpleOS2 several times on my own hardware and never encountered any damage. Nevertheless, I do not take any responsibility for any kind of damage to your system!

## Lessons learned
- When your Code works on Qemu but triple faults on VirtualBox, you have missed some very important minor detail in the AMD64 spec
- When your Code works on VirtualBox but triple faults on Qemu, chances are Qemu does not recognize that you've written to the KernelGSBase MSR (you have to write 0 to gs and fs)
- When your SMP Code crashes when booting more that two Processor Cores, they are probably booting on the same stack (took 2 days to find this stupid bug)
- *NEVER* *EVER* forget that stacks grow down. I spent multiple days searching seemingly random bugs, just to notice that I allocated a memory area for some stack and forgot that the stack should begin at the **end** of the allocated space
