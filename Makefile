# the exports below are all the tools that will be used in the building process

export PE_GCC := x86_64-w64-mingw32-gcc
export ELF_GCC := gcc
export NASM := nasm
export MTOOLS_MMD := mmd
export MTOOLS_MCOPY := mcopy
export DEBUGFS := debugfs
export PARTED := parted
export MKFS_FAT := mkfs.fat

export QEMU_X64 := qemu-system-x86_64

export VBOX_MANAGE := VBoxManage

# currently, only Debug and Release have special meaning
# the configuration is best changed by invoking make with "config=Release"
export config := Debug

# This is the main build directory, it should only be changed after "make clean" was called
export build_dir := $(shell pwd)/build
# This is the directory in which all binary files will be stored (should be inside the build_dir folder)
export bin_dir := $(build_dir)/$(config)/bin
# This is the directory in which all intermediate files will be stored (should be inside the build_dir folder)
export int_dir := $(build_dir)/$(config)/int

default: disk

depcheck: FORCE
	@ ./depcheck.sh

run: FORCE
	@ $(MAKE) -s disk
	@ printf "\e[32mStarting SimpleOS2 in Qemu\e[0m\n"
	@ $(QEMU_X64) -m 1024 -cpu qemu64 -smp 4 -net none -drive if=pflash,unit=0,format=raw,file=dep/ovmf/x64/OVMF_CODE.fd,readonly=on -drive if=pflash,unit=1,format=raw,file=dep/ovmf/x64/OVMF_VARS.fd,readonly=on -drive file=$(bin_dir)/SimpleOS2.img,if=ide
runvbox: FORCE
	@ $(MAKE) -s diskvbox
	@ printf "\e[32mStarting SimpleOS2 in VirtualBox\e[0m\n"
	@ rm -f $(build_dir)/SimpleOS2.vdi
	@ cp $(bin_dir)/SimpleOS2.vdi $(build_dir)/SimpleOS2.vdi
	@ $(VBOX_MANAGE) startvm SimpleOS2
debug: FORCE
	@ $(MAKE) -s disk
	@ printf "\e[32mDebugging SimpleOS2 in Qemu/GDB\e[0m\n"
	@ mkdir -p $(build_dir)/dbgSession
	@ rm -f $(build_dir)/dbgSession/Kernel.sys.dbg
	@ cp $(bin_dir)/Kernel/Kernel.sys.dbg $(build_dir)/dbgSession/Kernel.sys.dbg
	@ $(QEMU_X64) -gdb tcp::26000 -m 1024 -machine q35 -cpu qemu64 -net none -drive if=pflash,unit=0,format=raw,file=dep/ovmf/x64/OVMF_CODE.fd,readonly=on -drive if=pflash,unit=1,format=raw,file=dep/ovmf/x64/OVMF_VARS.fd,readonly=on -drive file=$(bin_dir)/SimpleOS2.img,if=ide -S & \
	  gdb --command=debug.cmd

bootloader: FORCE
	@ printf "\e[32mBuilding Bootloader\e[0m\n"
	@ $(MAKE) -s -C src/Bootloader
kernel: FORCE
	@ printf "\e[32mBuilding Kernel\e[0m\n"
	@ $(MAKE) -s -C src/Kernel
libc: FORCE
	@ printf "\e[32mBuilding LibC\e[0m\n"
	@ $(MAKE) -s -C src/libc
programs: FORCE
	@ $(MAKE) -s libc
	@ printf "\e[32mBuilding Init\e[0m\n"
	@ $(MAKE) -s -C src/Programs/Init
	@ printf "\e[32mBuilding TestVFS\e[0m\n"
	@ $(MAKE) -s -C src/Programs/TestVFS
rdbuilder: FORCE
	@ printf "\e[32mBuilding initrd tool\e[0m\n"
	@ $(MAKE) -s -C src/RamdiskBuilder
initrd: FORCE
	@ $(MAKE) -s rdbuilder
	@ $(MAKE) -s programs
	@ printf "\e[32mBuilding initrd\e[0m\n"
	@ $(MAKE) -s -f src/initrd.mk
partition: FORCE
	@ $(MAKE) -s bootloader
	@ $(MAKE) -s kernel
	@ $(MAKE) -s initrd
	@ printf "\e[32mBuilding SimpleOS2 boot partition\e[0m\n"
	@ $(MAKE) -s -f src/partition.mk
disk: FORCE
	@ $(MAKE) -s partition
	@ printf "\e[32mBuilding SimpleOS2 raw disk image\e[0m\n"
	@ $(MAKE) -s -f src/disk.mk
diskvbox: FORCE
	@ $(MAKE) -s disk
	@ printf "\e[32mBuilding SimpleOS2 vbox disk image\e[0m\n"
	@ $(MAKE) -s -f src/diskvbox.mk

clean: FORCE
	@ printf "\e[32mCleaning up\e[0m\n"
	@ rm -rf $(build_dir)

FORCE: 