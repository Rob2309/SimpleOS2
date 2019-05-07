export PE_GCC := x86_64-w64-mingw32-gcc
export ELF_GCC := gcc
export NASM := nasm

export config := Debug

export out_dir := $(shell pwd)
export bin_dir := $(out_dir)/bin/$(config)
export int_dir := $(out_dir)/int/$(config)
export RD_BUILDER = $(bin_dir)/tools/RamdiskBuilder

root_partition_img_deps := $(bin_dir)/Kernel/Kernel.sys $(bin_dir)/Bootloader/BOOTX64.EFI
ramdisk_files := $(bin_dir)/Programs/Test/test.elf $(bin_dir)/Programs/Test2/test2.elf
ramdisk_command := $(bin_dir)/Programs/Test/test.elf test.elf $(bin_dir)/Programs/Test2/test2.elf test2.elf

depcheck: FORCE
	./depcheck.sh

run: $(bin_dir)/partition.img FORCE
	qemu-system-x86_64 -m 1024 -cpu qemu64 -net none -drive if=pflash,unit=0,format=raw,file=dep/ovmf/x64/OVMF_CODE.fd,readonly=on -drive if=pflash,unit=1,format=raw,file=dep/ovmf/x64/OVMF_VARS.fd,readonly=on -drive file=$<,if=ide
runvbox: $(bin_dir)../partition.vdi FORCE
	VBoxManage startvm SimpleOS2

debug: $(bin_dir)/partition.img FORCE
	qemu-system-x86_64 -gdb tcp::26000 -m 1024 -cpu qemu64 -net none -drive if=pflash,unit=0,format=raw,file=dep/ovmf/x64/OVMF_CODE.fd,readonly=on -drive if=pflash,unit=1,format=raw,file=dep/ovmf/x64/OVMF_VARS.fd,readonly=on -drive file=$<,if=ide -S & \
	gdb --command=debug.cmd

$(bin_dir)/../partition.vdi: $(bin_dir)/partition.vdi
	rm -f $@
	cp $< $@

$(bin_dir)/partition.vdi: $(bin_dir)/partition.img
	rm -rf $@
	VBoxManage convertfromraw $< $@ --format VDI --uuid 430eee2a-0fdf-4d2a-88f0-5b99ea8cffca

$(bin_dir)/partition.img: $(root_partition_img_deps) $(bin_dir)/initrd
	dd if=/dev/zero of=$@ bs=512 count=102400
	mkfs.fat $@
	mmd -i $@ ::/EFI
	mmd -i $@ ::/EFI/BOOT
	mcopy -i $@ $^ ::/EFI/BOOT

$(bin_dir)/initrd: $(RD_BUILDER) $(ramdisk_files)
	$(RD_BUILDER) $@ $(ramdisk_command)
$(bin_dir)/Kernel/Kernel.sys: FORCE
	make -C src/Kernel
$(bin_dir)/Programs/Test/test.elf: FORCE
	make -C src/Programs/Test
$(bin_dir)/Programs/Test2/test2.elf: FORCE
	make -C src/Programs/Test2
$(bin_dir)/Bootloader/BOOTX64.EFI: FORCE
	make -C src/Bootloader

$(RD_BUILDER): FORCE
	make -C src/RamdiskBuilder 

clean: FORCE
	rm -rf bin int

FORCE: 
