export PE_GCC := x86_64-w64-mingw32-gcc
export ELF_GCC := gcc
export NASM := nasm

root_partition_img_deps := 
clean_files := bin int tools
include_paths := 

ramdisk_files := 
ramdisk_command := 

include src/RamdiskBuilder/module.mk

include src/Bootloader/module.mk
include src/Kernel/module.mk
include src/Programs/Test/module.mk

depcheck: FORCE
	./depcheck.sh

run: bin/partition.img FORCE
	qemu-system-x86_64 -m 1024 -cpu qemu64 -net none -drive if=pflash,unit=0,format=raw,file=dep/ovmf/x64/OVMF_CODE.fd,readonly=on -drive if=pflash,unit=1,format=raw,file=dep/ovmf/x64/OVMF_VARS.fd,readonly=on -drive file=$<,if=ide
runvbox: bin/partition.vdi FORCE
	VBoxManage startvm SimpleOS2

debug: bin/partition.img FORCE
	qemu-system-x86_64 -gdb tcp::26000 -m 1024 -cpu qemu64 -net none -drive if=pflash,unit=0,format=raw,file=dep/ovmf/x64/OVMF_CODE.fd,readonly=on -drive if=pflash,unit=1,format=raw,file=dep/ovmf/x64/OVMF_VARS.fd,readonly=on -drive file=$<,if=ide -S & \
	gdb --command=debug.cmd
debugvbox: bin/partition.vdi FORCE
	VBoxManage startvm SimpleOS2 debug

bin/partition.vdi: bin/partition.img
	rm -rf $@
	VBoxManage convertfromraw $< $@ --format VDI --uuid 430eee2a-0fdf-4d2a-88f0-5b99ea8cffca

bin/partition.img: $(root_partition_img_deps) bin/initrd
	dd if=/dev/zero of=$@ bs=512 count=102400
	mkfs.fat $@
	mmd -i $@ ::/EFI
	mmd -i $@ ::/EFI/BOOT
	mcopy -i $@ $^ ::/EFI/BOOT

bin/initrd: $(RDBUILDER) $(ramdisk_files)
	$(RDBUILDER) $@ $(ramdisk_command)

clean: FORCE
	rm -rf $(clean_files)

FORCE: 
