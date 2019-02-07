export PE_GCC := x86_64-w64-mingw32-gcc
export ELF_GCC := gcc
export NASM := nasm

root_partition_img_deps := 
clean_files := *.img *.vdi
include_paths := 

include Bootloader/module.mk
include Kernel/module.mk

run: partition.img FORCE
	qemu-system-x86_64 -m 1024 -cpu qemu64 -net none -drive if=pflash,unit=0,format=raw,file=dep/ovmf/x64/OVMF_CODE.fd,readonly=on -drive if=pflash,unit=1,format=raw,file=dep/ovmf/x64/OVMF_VARS.fd,readonly=on -drive file=$<,if=ide
runvbox: partition.vdi FORCE
	VBoxManage startvm SimpleOS2

debug: partition.img FORCE
	qemu-system-x86_64 -gdb tcp::26000 -m 1024 -cpu qemu64 -net none -drive if=pflash,unit=0,format=raw,file=dep/ovmf/x64/OVMF_CODE.fd,readonly=on -drive if=pflash,unit=1,format=raw,file=dep/ovmf/x64/OVMF_VARS.fd,readonly=on -drive file=$<,if=ide &
debugvbox: partition.vdi FORCE
	VBoxManage startvm SimpleOS2 debug

partition.vdi: partition.img
	rm -rf $@
	VBoxManage convertfromraw $< $@ --format VDI --uuid 430eee2a-0fdf-4d2a-88f0-5b99ea8cffca

partition.img: $(root_partition_img_deps)
	dd if=/dev/zero of=$@ bs=512 count=102400
	mkfs.fat $@
	mmd -i $@ ::/EFI
	mmd -i $@ ::/EFI/BOOT
	mcopy -i $@ $^ ::/EFI/BOOT

clean: FORCE
	rm -rf ${clean_files}

FORCE: 
