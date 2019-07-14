#!/bin/bash

export PE_GCC := x86_64-w64-mingw32-gcc
export ELF_GCC := gcc
export NASM := nasm

export config := Debug

export out_dir := $(shell pwd)
export bin_dir_rel := bin/$(config)
export bin_dir := $(out_dir)/$(bin_dir_rel)
export int_dir := $(out_dir)/int/$(config)
export RD_BUILDER = $(bin_dir)/tools/RamdiskBuilder

programs := $(subst src/Programs/,,$(shell find src/Programs -mindepth 1 -maxdepth 1 -type d -printf "%p "))

root_partition_img_deps := $(bin_dir)/Kernel/Kernel.sys $(bin_dir)/Bootloader/BOOTX64.EFI $(bin_dir)/initrd

ramdisk_files := $(foreach program,$(programs),$(bin_dir)/Programs/$(program)/$(program).elf)
ramdisk_command := $(foreach program,$(programs),$(bin_dir)/Programs/$(program)/$(program).elf $(program).elf)

depcheck: FORCE
	@ ./depcheck.sh

disk: $(bin_dir)/SimpleOS2.img FORCE
vboxdisk: $(bin_dir)/SimpleOS2.vdi FORCE
run: $(bin_dir)/SimpleOS2.img FORCE
	@ printf "\e[32mstarting SimpleOS2 in qemu\e[0m\n"
	@ qemu-system-x86_64 -m 1024 -cpu qemu64 -smp 4 -net none -drive if=pflash,unit=0,format=raw,file=dep/ovmf/x64/OVMF_CODE.fd,readonly=on -drive if=pflash,unit=1,format=raw,file=dep/ovmf/x64/OVMF_VARS.fd,readonly=on -drive file=$<,if=ide
runvbox: $(bin_dir)/../SimpleOS2.vdi FORCE
	@ printf "\e[32mstarting SimpleOS2 in VirtualBox\e[0m\n"
	@ VBoxManage startvm SimpleOS2

debug: $(bin_dir)/SimpleOS2.img FORCE
	@ printf "\e[32mStarting debugging session\e[0m\n"
	@ qemu-system-x86_64 -gdb tcp::26000 -m 1024 -cpu qemu64 -net none -drive if=pflash,unit=0,format=raw,file=dep/ovmf/x64/OVMF_CODE.fd,readonly=on -drive if=pflash,unit=1,format=raw,file=dep/ovmf/x64/OVMF_VARS.fd,readonly=on -drive file=$<,if=ide -S & \
	  gdb --command=debug.cmd

$(bin_dir)/../SimpleOS2.vdi: $(bin_dir)/SimpleOS2.vdi
	@ rm -f $@
	@ cp $< $@

$(bin_dir)/SimpleOS2.vdi: $(bin_dir)/SimpleOS2.img
	@ printf "\e[32mConverting raw disk image into VirtualBox disk image\e[0m\n"
	@ rm -rf $@
	@ VBoxManage convertfromraw $(bin_dir_rel)/SimpleOS2.img $(bin_dir_rel)/SimpleOS2.vdi --format VDI --uuid 430eee2a-0fdf-4d2a-88f0-5b99ea8cffca

$(bin_dir)/partition.img: $(root_partition_img_deps)
	@ printf "\e[32mBuilding raw partition\e[0m\n"
	@ dd if=/dev/zero of=$@ bs=512 count=102400 2>/dev/null
	@ mkfs.fat $@ >/dev/null
	@ mmd -i $@ ::/EFI
	@ mmd -i $@ ::/EFI/BOOT
	@ mcopy -i $@ $^ ::/EFI/BOOT

$(bin_dir)/SimpleOS2.img: $(bin_dir)/partition.img
	@ printf "\e[32mBuilding SimpleOS2 disk image\e[0m\n"
	@ dd if=/dev/zero of=$@ bs=512 count=110000 2>/dev/null
	@ parted $@ mklabel gpt mkpart SimpleOS2 fat32 2048s 104447s toggle 1 esp
	@ dd if=$< of=$@ bs=512 conv=notrunc seek=2048

$(bin_dir)/initrd: $(RD_BUILDER) $(ramdisk_files)
	@ printf "\e[32mBuilding ramdisk\e[0m\n"
	@ $(RD_BUILDER) $@ $(ramdisk_command)
$(bin_dir)/Kernel/Kernel.sys: FORCE
	@ printf "\e[32mBuilding kernel\e[0m\n"
	@ make -s -C src/Kernel
$(bin_dir)/libc/libc.a: FORCE
	@ printf "\e[32mBuilding libc\e[0m\n"
	@ make -s -C src/libc
$(bin_dir)/Programs/Init/Init.elf: $(bin_dir)/libc/libc.a FORCE
	@ printf "\e[32mBuilding Init\e[0m\n"
	@ make -s -C src/Programs/Init
$(bin_dir)/Programs/Test2/Test2.elf: $(bin_dir)/libc/libc.a FORCE
	@ printf "\e[32mBuilding Test2\e[0m\n"
	@ make -s -C src/Programs/Test2
$(bin_dir)/Bootloader/BOOTX64.EFI: FORCE
	@ printf "\e[32mBuilding bootloader\e[0m\n"
	@ make -s -C src/Bootloader

$(RD_BUILDER): FORCE
	@ printf "\e[32mBuilding RamdiskBuilder tool\e[0m\n"
	@ make -s -C src/RamdiskBuilder 

clean: FORCE
	@ printf "\e[32mCleaning up\e[0m\n"
	@ rm -rf bin int

FORCE: 
