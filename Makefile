export PE_GCC := x86_64-w64-mingw32-gcc
export ELF_GCC := gcc
export NASM := nasm

root_partition_img_deps := 
clean_files := bin int tools
include_paths := 

ramdisk_files := 
ramdisk_command := 

config := dbg

include src/RamdiskBuilder/module.mk

include src/Bootloader/module.mk
include src/Kernel/module.mk
include src/Programs/Test/module.mk

depcheck: FORCE
	./depcheck.sh

run: partition FORCE
	qemu-system-x86_64 -m 1024 -cpu qemu64 -net none -drive if=pflash,unit=0,format=raw,file=dep/ovmf/x64/OVMF_CODE.fd,readonly=on -drive if=pflash,unit=1,format=raw,file=dep/ovmf/x64/OVMF_VARS.fd,readonly=on -drive file=bin/$(config)/partition.img,if=ide
runvbox: vboxpartition FORCE
	make env_config_$(config); make bin/$(config)/partition.vdi
	rm -rf bin/partition.vdi
	cp bin/$(config)/partition.vdi bin/partition.vdi
	VBoxManage startvm SimpleOS2

debug: partition FORCE
	make env_config_$(config); make bin/$(config)/partition.img
	qemu-system-x86_64 -gdb tcp::26000 -m 1024 -cpu qemu64 -net none -drive if=pflash,unit=0,format=raw,file=dep/ovmf/x64/OVMF_CODE.fd,readonly=on -drive if=pflash,unit=1,format=raw,file=dep/ovmf/x64/OVMF_VARS.fd,readonly=on -drive file=bin/$(config)/partition.img,if=ide -S & \
	gdb --command=debug.cmd

partition: FORCE
	if [ "$(config)" = "dbg" ];     then export compile_defs="-DDEBUG"; fi; \
	if [ "$(config)" = "release" ]; then export compile_defs="-DRELEASE -O1"; fi; \
	make bin/$(config)/partition.img

vboxpartition: FORCE
	make partition
	make bin/$(config)/partition.vdi

bin/$(config)/partition.vdi: bin/$(config)/partition.img
	rm -rf $@
	VBoxManage convertfromraw $< $@ --format VDI --uuid 430eee2a-0fdf-4d2a-88f0-5b99ea8cffca

bin/$(config)/partition.img: $(root_partition_img_deps) bin/$(config)/initrd
	dd if=/dev/zero of=$@ bs=512 count=102400
	mkfs.fat $@
	mmd -i $@ ::/EFI
	mmd -i $@ ::/EFI/BOOT
	mcopy -i $@ $^ ::/EFI/BOOT

bin/$(config)/initrd: $(RDBUILDER) $(ramdisk_files)
	$(RDBUILDER) $@ $(ramdisk_command)

clean: FORCE
	rm -rf $(clean_files)

FORCE: 
